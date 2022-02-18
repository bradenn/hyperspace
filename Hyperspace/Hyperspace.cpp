/*
 * Copyright (C) 2005-2022  Terence M. Welsh & Braden Nicholson
 *
 * This file is part of Hyperspace.
 *
 * Hyperspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * Hyperspace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <rsText/rsText.h>
#include <math.h>
#include <time.h>

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <rsMath/rsMath.h>
#include <Hyperspace/flare.h>
#include <Hyperspace/causticTextures.h>
#include <Hyperspace/wavyNormalCubeMaps.h>
#include <Hyperspace/splinePath.h>
#include <Hyperspace/tunnel.h>
#include <Hyperspace/goo.h>
#include <Hyperspace/stretchedParticle.h>
#include <Hyperspace/starBurst.h>
#include <Hyperspace/nebulamap.h>
#include <Hyperspace/shaders.h>
#include "Hyperspace.h"

#define GL_SILENCE_DEPRECATION

float timeMS() {
    time_t now = time(nullptr);
    return now * 1000.0f;
}

static float lastUpdate = timeMS();

void draw(HyperspaceSaverSettings *inSettings) {
    if (inSettings->first) {
        if (inSettings->dShaders) {
            inSettings->theWNCM = new wavyNormalCubeMaps(inSettings->numAnimTexFrames, 128);
        }
        glViewport(inSettings->viewport[0], inSettings->viewport[1], inSettings->viewport[2], inSettings->viewport[3]);
        inSettings->first = 0;
    }

    // Variables for printing text
    static float computeTime = 0.0f;
    static float drawTime = 0.0f;

    glMatrixMode(GL_MODELVIEW);

    // Camera movements
    static float camHeading[3] = {0.0f, 0.0f, 0.0f};  // current, target, and last

    static float pathDir[3] = {0.0f, 0.0f, -1.0f};

    inSettings->camPos[2] = inSettings->camPos[2] - (inSettings->dSpeed * 0.0005f);

    float pathAngle = atan2f(-pathDir[0], -pathDir[2]);

    glLoadIdentity();
    glRotatef((pathAngle + camHeading[0]) * RS_RAD2DEG, 0, 1, 0);
    glRotatef(0, 0, 0, 1);
    glGetFloatv(GL_MODELVIEW_MATRIX, inSettings->billboardMat);
    glLoadIdentity();
    glRotatef(-0, 0, 0, 1);
    glRotatef((-pathAngle - camHeading[0]) * RS_RAD2DEG, 0, 1, 0);
    glTranslatef(inSettings->camPos[0] * -1, inSettings->camPos[1] * -1, inSettings->camPos[2] * -1);
    glGetDoublev(GL_MODELVIEW_MATRIX, inSettings->modelMat);
    inSettings->unroll = 0.0f;

    if (inSettings->dUseGoo) {
        // calculate diagonal fov
        float diagFov = 0.5f * float(inSettings->dFov) / RS_RAD2DEG;
        diagFov = tanf(diagFov);
        diagFov = sqrtf(diagFov * diagFov + (diagFov * inSettings->aspectRatio * diagFov * inSettings->aspectRatio));
        diagFov = 2.0f * atanf(diagFov);
        inSettings->theGoo->update(inSettings->camPos[0], inSettings->camPos[2], pathAngle + camHeading[0], diagFov,
                                   inSettings);
    }

    // measure compute time
    //computeTime += computeTimer.tick();
    // start draw time timer
    //drawTimer.tick();

    // clear
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw stars
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glActiveTextureARB(GL_TEXTURE2_ARB);
    glBindTexture(GL_TEXTURE_2D, NULL);
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glBindTexture(GL_TEXTURE_2D, NULL);
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glBindTexture(GL_TEXTURE_2D, inSettings->flaretex[0]);
    static float temppos[2];
    for (int i = 0; i < inSettings->dStars; i++) {
        temppos[0] = inSettings->stars[i]->pos[0] - inSettings->camPos[0];
        temppos[1] = inSettings->stars[i]->pos[2] - inSettings->camPos[2];
        if (temppos[0] > inSettings->depth) {
            inSettings->stars[i]->pos[0] -= inSettings->depth * 2.0f;
            inSettings->stars[i]->lastPos[0] -= inSettings->depth * 2.0f;
        }
        if (temppos[0] < inSettings->depth * -1) {
            inSettings->stars[i]->pos[0] += inSettings->depth * 2.0f;
            inSettings->stars[i]->lastPos[0] += inSettings->depth * 2.0f;
        }
        if (temppos[1] > inSettings->depth) {
            inSettings->stars[i]->pos[2] -= inSettings->depth * 2.0f;
            inSettings->stars[i]->lastPos[2] -= inSettings->depth * 2.0f;
        }
        if (temppos[1] < inSettings->depth * -1) {
            inSettings->stars[i]->pos[2] += inSettings->depth * 2.0f;
            inSettings->stars[i]->lastPos[2] += inSettings->depth * 2.0f;
        }
        inSettings->stars[i]->draw(inSettings->camPos, inSettings->unroll, inSettings->modelMat, inSettings->projMat,
                                   inSettings->viewport);
    }
    glDisable(GL_CULL_FACE);

    // pick animated texture frame
    static float textureTime = 0.0f;
    textureTime += inSettings->frameTime;
    // loop frames every 2 seconds
    const float texFrameTime(2.0f / float(inSettings->numAnimTexFrames));

    while (textureTime > texFrameTime) {
        textureTime -= texFrameTime;
        inSettings->whichTexture++;
    }

    while (inSettings->whichTexture >= inSettings->numAnimTexFrames)
        inSettings->whichTexture -= inSettings->numAnimTexFrames;

    // alpha component gets normalmap lerp value
    const float lerp = textureTime / texFrameTime;

    // draw goo
    if (inSettings->dUseGoo) {
        // calculate color
        static float goo_rgb_phase[3] = {-0.1f, -0.1f, -0.1f};
        static float goo_rgb_speed[3] = {rsRandf(0.01f) + 0.01f, rsRandf(0.01f) + 0.01f, rsRandf(0.01f) + 0.01f};
        float goo_rgb[4];
        for (int i = 0; i < 3; i++) {
            goo_rgb_phase[i] += goo_rgb_speed[i] * inSettings->frameTime;
            if (goo_rgb_phase[i] >= RS_PIx2)
                goo_rgb_phase[i] -= RS_PIx2;
            goo_rgb[i] = sinf(goo_rgb_phase[i]);
            if (goo_rgb[i] < 0.0f)
                goo_rgb[i] = 0.0f;
        }
        // setup textures
        if (inSettings->dShaders) {
            goo_rgb[3] = lerp;
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_TEXTURE_CUBE_MAP_ARB);
            glActiveTextureARB(GL_TEXTURE2_ARB);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, inSettings->nebulatex);
            glActiveTextureARB(GL_TEXTURE1_ARB);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARB,
                          inSettings->theWNCM->texture[(inSettings->whichTexture + 1) % inSettings->numAnimTexFrames]);
            glActiveTextureARB(GL_TEXTURE0_ARB);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, inSettings->theWNCM->texture[inSettings->whichTexture]);
            glUseProgramObjectARB(gooProgram);
        } else {
            goo_rgb[3] = 1.0f;
            glBindTexture(GL_TEXTURE_2D, inSettings->nebulatex);
            glEnable(GL_TEXTURE_2D);
            glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
            glEnable(GL_TEXTURE_GEN_S);
            glEnable(GL_TEXTURE_GEN_T);
        }
        // draw it
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glEnable(GL_BLEND);
        glColor4fv(goo_rgb);
        inSettings->theGoo->draw();
        if (inSettings->dShaders) {
            glDisable(GL_TEXTURE_CUBE_MAP_ARB);
            glUseProgramObjectARB(0);
        } else {
            glDisable(GL_TEXTURE_GEN_S);
            glDisable(GL_TEXTURE_GEN_T);
        }
    }

    // update starburst
    static float starBurstTime = 300.0f;  // burst after 5 minutes
    starBurstTime -= inSettings->frameTime;

    if (starBurstTime <= 0.0f) {
        float pos[] = {inSettings->camPos[0] + (pathDir[0] * inSettings->depth * (0.5f + rsRandf(0.5f))),
                       rsRandf(2.0f) - 1.0f,
                       inSettings->camPos[2] + (pathDir[2] * inSettings->depth * (0.5f + rsRandf(0.5f)))};
        inSettings->theStarBurst->restart(pos);  // it won't actually restart unless it's ready to
        starBurstTime = rsRandf(240.0f) + 60.0f;  // burst again within 1-5 minutes
    }

    if (inSettings->dShaders)
        inSettings->theStarBurst->draw(lerp, inSettings);
    else
        inSettings->theStarBurst->draw(inSettings);


    // draw sun with lens flare
    glDisable(GL_FOG);
    float flarepos[3] = {0.0f, 2.0f, 0.0f};
    glBindTexture(GL_TEXTURE_2D, inSettings->flaretex[0]);
    inSettings->sunStar->draw(inSettings->camPos, inSettings->unroll, inSettings->modelMat, inSettings->projMat,
                              inSettings->viewport);
    float diff[3] = {flarepos[0] - inSettings->camPos[0], flarepos[1] - inSettings->camPos[1],
                     flarepos[2] - inSettings->camPos[2]};
    float alpha = 0.5f - 0.005f * sqrtf(diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2]);

    if (alpha > 0.0f)
        flare(flarepos, 1.0f, 1.0f, 1.0f, alpha, inSettings);


    glEnable(GL_FOG);

    // write text
    static float totalTime = 0.0f;
    totalTime += inSettings->frameTime;
    static std::vector <std::string> strvec;

    static int frames = 0;
    ++frames;
    if (frames == 60) {
        strvec.clear();
        std::string str1 = "         FPS = " + to_string(60.0f / totalTime);
        strvec.push_back(str1);
        std::string str2 = "compute time = " + to_string(computeTime / 60.0f);
        strvec.push_back(str2);
        std::string str3 = "   draw time = " + to_string(drawTime / 60.0f);
        strvec.push_back(str3);
        totalTime = 0.0f;
        computeTime = 0.0f;
        drawTime = 0.0f;
        frames = 0;
    }

    if (inSettings->kStatistics) {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0f, 50.0f * inSettings->aspectRatio, 0.0f, 50.0f, -1.0f, 1.0f);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(1.0f, 48.0f, 0.0f);

        glColor3f(1.0f, 0.6f, 0.0f);
        inSettings->textwriter->draw(strvec);

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }
}

void setDefaults(HyperspaceSaverSettings *inSettings) {
    inSettings->dSpeed = 4;
    inSettings->dStars = 6400;
    inSettings->dStarSize = 8;
    inSettings->dResolution = 16;
    inSettings->dDepth = 4;
    inSettings->dFov = 90;
    inSettings->dUseTunnels = 0;
    inSettings->dUseGoo = 0;
    inSettings->dShaders = 1;
}

void reshape(int width, int height, HyperspaceSaverSettings *inSettings) {
    glViewport(0, 0, width, height);
    inSettings->aspectRatio = float(width) / float(height);

    inSettings->xsize = width;
    inSettings->ysize = height;

    inSettings->viewport[0] = 0;
    inSettings->viewport[1] = 0;
    inSettings->viewport[2] = width;
    inSettings->viewport[3] = height;

    // setup projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(float(inSettings->dFov), inSettings->aspectRatio, 0.001f, 200.0f);
    glGetDoublev(GL_PROJECTION_MATRIX, inSettings->projMat);
    glMatrixMode(GL_MODELVIEW);
}

void initSaver(HyperspaceSaverSettings *inSettings) {
    // Seed random number generator
    srand((unsigned) time(NULL));
    // Set up some other inSettings defaults...
    inSettings->camPos[0] = 0.0f;
    inSettings->camPos[1] = 0.0f;
    inSettings->camPos[2] = 0.0f;
    inSettings->numAnimTexFrames = 24;
    inSettings->whichTexture = 0;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    initFlares(inSettings);

    // To avoid popping, depth, which will be used for fogging, is set to
    // dDepth * goo grid size - size of one goo cubelet
    inSettings->depth = float(inSettings->dDepth) * 2.0f - 2.0f / float(inSettings->dResolution);

    if (inSettings->dUseGoo)
        inSettings->theGoo = new goo(inSettings->dResolution, inSettings->depth);

    inSettings->stars = new stretchedParticle *[inSettings->dStars];


    for (int i = 0; i < inSettings->dStars; i++) {
        inSettings->stars[i] = new stretchedParticle;
        inSettings->stars[i]->radius =
                rsRandf(float(inSettings->dStarSize) * 0.0005f) + float(inSettings->dStarSize) * 0.0005f;
        if (i % 10){
            inSettings->stars[i]->color[0] = 0.9f+rsRandf(0.1f);
            inSettings->stars[i]->color[1] = 0.78f + rsRandf(0.2f);
            inSettings->stars[i]->color[2] = 0.7f + rsRandf(0.1f);
        } else {
            inSettings->stars[i]->color[0] = 0.9f + rsRandf(0.1f);
            inSettings->stars[i]->color[1] = 0.5f + rsRandf(0.1f);
            inSettings->stars[i]->color[2] = 0.1f + rsRandf(0.1f);
        }

        inSettings->stars[i]->pos[0] = rsRandf(2.0f * inSettings->depth) - inSettings->depth;
        inSettings->stars[i]->pos[1] = rsRandf(2.0f * inSettings->depth) - inSettings->depth;
        inSettings->stars[i]->pos[2] = rsRandf(2.0f * inSettings->depth) - inSettings->depth;
        inSettings->stars[i]->fov = float(inSettings->dFov);
    }

    inSettings->sunStar = new stretchedParticle;
    inSettings->sunStar->radius = float(inSettings->dStarSize) * 0.004f;
    inSettings->sunStar->pos[0] = 0.0f;
    inSettings->sunStar->pos[1] = 2.0f;
    inSettings->sunStar->pos[2] = 0.0f;
    inSettings->sunStar->fov = float(inSettings->dFov);

    inSettings->theStarBurst = new starBurst;
    for (int i = 0; i < SB_NUM_STARS; i++)
        inSettings->theStarBurst->stars[i]->radius =
                rsRandf(float(inSettings->dStarSize) * 0.001f) + float(inSettings->dStarSize) * 0.001f;

    glGenTextures(1, &inSettings->nebulatex);
    if (inSettings->dShaders) {
        initShaders();
        inSettings->numAnimTexFrames = 24;
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, inSettings->nebulatex);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE,
                          nebulamap);
        gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE,
                          nebulamap);
        gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE,
                          nebulamap);
        gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE,
                          nebulamap);
        gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE,
                          nebulamap);
        gluBuild2DMipmaps(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE,
                          nebulamap);
    } else {
        inSettings->numAnimTexFrames = 24;
        float x, y, temp;
        const int halfsize(NEBULAMAPSIZE / 2);
        for (int i = 0; i < NEBULAMAPSIZE; ++i) {
            for (int j = 0; j < NEBULAMAPSIZE; ++j) {
                x = float(i - halfsize) / float(halfsize);
                y = float(j - halfsize) / float(halfsize);
                temp = (x * x) + (y * y);
                if (temp > 1.0f)
                    temp = 1.0f;
                if (temp < 0.0f)
                    temp = 0.0f;
                temp = temp * temp;
                temp = temp * temp;
                nebulamap[i][j][0] = GLubyte(float(nebulamap[i][j][0]) * temp);
                nebulamap[i][j][1] = GLubyte(float(nebulamap[i][j][1]) * temp);
                nebulamap[i][j][2] = GLubyte(float(nebulamap[i][j][2]) * temp);
            }
        }
        glEnable(GL_NORMALIZE);
        glBindTexture(GL_TEXTURE_2D, inSettings->nebulatex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        gluBuild2DMipmaps(GL_TEXTURE_2D, 3, NEBULAMAPSIZE, NEBULAMAPSIZE, GL_RGB, GL_UNSIGNED_BYTE, nebulamap);
    }

    glEnable(GL_FOG);
    float fog_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, inSettings->depth * 0.7f);
    glFogf(GL_FOG_END, inSettings->depth);

    // Initialize text
    inSettings->textwriter = new rsText;

    inSettings->readyToDraw = 1;
}

void cleanUp(HyperspaceSaverSettings *inSettings) {
    // Free memory
    if (inSettings->dUseGoo) delete inSettings->theGoo;

    delete inSettings->thePath;
    delete inSettings->theWNCM;

    inSettings->first = 1;
}
