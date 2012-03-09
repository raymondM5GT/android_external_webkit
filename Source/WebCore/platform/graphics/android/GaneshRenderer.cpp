/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "GaneshRenderer"
#define LOG_NDEBUG 1

#include "config.h"
#include "GaneshRenderer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "AndroidLog.h"
#include "GaneshContext.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include "TilesManager.h"

namespace WebCore {

static const String TAG_CREATE_FBO = "create_fbo";
static const String TAG_DRAW_PICTURE = "draw_picture";
static const String TAG_UPDATE_TEXTURE = "update_texture";
#define TAG_COUNT 3
static const String TAGS[] = {
    TAG_CREATE_FBO,
    TAG_DRAW_PICTURE,
    TAG_UPDATE_TEXTURE,
};

GaneshRenderer::GaneshRenderer() : BaseRenderer(BaseRenderer::Ganesh)
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->increment("GaneshRenderer");
#endif
}

GaneshRenderer::~GaneshRenderer()
{
#ifdef DEBUG_COUNT
    ClassTracker::instance()->decrement("GaneshRenderer");
#endif
}

void GaneshRenderer::setupCanvas(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (renderInfo.measurePerf)
        m_perfMon.start(TAG_CREATE_FBO);

    GaneshContext* ganesh = GaneshContext::instance();

    TransferQueue* tileQueue = TilesManager::instance()->transferQueue();

    tileQueue->lockQueue();

    bool ready = tileQueue->readyForUpdate();
    if (!ready) {
        ALOGV("!ready");
        tileQueue->unlockQueue();
        return;
    }

    SkDevice* device = NULL;
    if (renderInfo.tileSize.width() == TilesManager::tileWidth()
            && renderInfo.tileSize.height() == TilesManager::tileHeight()) {
        device = ganesh->getDeviceForBaseTile(renderInfo);
    } else {
        // TODO support arbitrary sizes for layers
        ALOGV("ERROR: expected (%d,%d) actual (%d,%d)",
              TilesManager::tileWidth(), TilesManager::tileHeight(),
              renderInfo.tileSize.width(), renderInfo.tileSize.height());
    }

    if (renderInfo.measurePerf) {
        m_perfMon.stop(TAG_CREATE_FBO);
        m_perfMon.start(TAG_DRAW_PICTURE);
    }

    // set the GPU device to the canvas
    canvas->setDevice(device);
}

void GaneshRenderer::setupPartialInval(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    // set the clip to our invalRect
    SkRect clipRect = SkRect::MakeLTRB(renderInfo.invalRect->fLeft,
                                       renderInfo.invalRect->fTop,
                                       renderInfo.invalRect->fRight,
                                       renderInfo.invalRect->fBottom);
    canvas->clipRect(clipRect);
}

void GaneshRenderer::renderingComplete(const TileRenderInfo& renderInfo, SkCanvas* canvas)
{
    if (renderInfo.measurePerf) {
        m_perfMon.stop(TAG_DRAW_PICTURE);
        m_perfMon.start(TAG_UPDATE_TEXTURE);
    }

    ALOGV("rendered to tile (%d,%d)", renderInfo.x, renderInfo.y);

    GaneshContext::instance()->flush();

    // In SurfaceTextureMode we must call swapBuffers to unlock and post the
    // tile's ANativeWindow (i.e. SurfaceTexture) buffer
    TransferQueue* tileQueue = TilesManager::instance()->transferQueue();
    eglSwapBuffers(eglGetCurrentDisplay(), tileQueue->m_eglSurface);
    tileQueue->addItemInTransferQueue(&renderInfo, GpuUpload, 0);
    tileQueue->unlockQueue();

    if (renderInfo.measurePerf)
        m_perfMon.stop(TAG_UPDATE_TEXTURE);
}

const String* GaneshRenderer::getPerformanceTags(int& tagCount)
{
    tagCount = TAG_COUNT;
    return TAGS;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
