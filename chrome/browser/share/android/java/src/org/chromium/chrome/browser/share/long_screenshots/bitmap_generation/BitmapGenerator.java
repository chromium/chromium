// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.url.GURL;

/**
 * Class responsible for processing the initial request, calling {@link LongScreenshotsTab} service
 * to capture the webpage, then using {@link LongScreenshotsCompositor} to composite the bitmap.
 * Callers of this class should supply a GeneratorCallback to receive status updates.
 */
public class BitmapGenerator implements LongScreenshotsTabService.CaptureProcessor {
    // Response with a pointer to the skia image
    private PaintPreviewProto mProtoResponse;

    // Compositor delegate responsible for compositing the skia
    private LongScreenshotsCompositor mCompositor;
    private LongScreenshotsTabService mTabService;
    private Tab mTab;

    private static final String DIR_NAME = "long_screenshots_dir";

    protected GeneratorCallBack mGeneratorCallBack;
    private ScreenshotBoundsManager mBoundsManager;

    /**
     * Users of the {@link LongScreenshotsEntry} class have to implement and pass this interface in
     * the constructor.
     */
    public interface GeneratorCallBack {
        /**
         * Called when the compositor cannot be successfully initialized.
         */
        void onCompositorResult(@CompositorStatus int status);

        /**
         * Called when the capture is complete.
         */
        void onCaptureResult(@Status int status);
    }

    /**
     * @param tab The current tab being screen-shotted.
     * @param rect The area of the webpage to capture
     * @param callback Callback to receive updates from the generation.
     */
    public BitmapGenerator(
            Tab tab, ScreenshotBoundsManager boundsManager, GeneratorCallBack callback) {
        mTab = tab;
        mBoundsManager = boundsManager;
        mGeneratorCallBack = callback;
    }

    /**
     * Starts the capture of the screenshot.
     */
    public void captureTab() {
        if (mTabService == null) {
            mTabService = LongScreenshotsTabServiceFactory.getServiceInstance();
        }
        mTabService.setCaptureProcessor(this);
        mTabService.captureTab(mTab, mBoundsManager.getCaptureBounds());
    }

    /**
     * Called from native after the tab has been captured. If status is OK, then calls the
     * compositor on the response. Otherwise, calls the GeneratorCallback with the status.
     *
     * @param response Response with details about the capture.
     * @param status Status of the capture.
     */
    @Override
    public void processCapturedTab(PaintPreviewProto response, @Status int status) {
        if (status == Status.OK && mCompositor == null) {
            mCompositor = new LongScreenshotsCompositor(new GURL(response.getMetadata().getUrl()),
                    mTabService, DIR_NAME, response, this::onCompositorResult);
        } else {
            onCaptureResult(status);
        }
    }

    /**
     * Composites the capture into a bitmap for the bounds defined. Callers should wait for the
     * onBitmapGeneratedFunction to be called with the generated Bitmap.
     *
     * @param rect The bounds of the webpage (not capture) to composite into bitmap.
     * @param errorCallback Callback for when an error is encountered
     * @param onBitmapGenerated Called with the generated bitmap.
     * @return id of the request.
     */
    public int compositeBitmap(
            Rect rect, Runnable errorCallback, Callback<Bitmap> onBitmapGenerated) {
        // Check if the compositor is ready and whether the rect is within the bounds of the
        // the capture.
        return mCompositor.requestBitmap(mBoundsManager.calculateBoundsRelativeToCapture(rect),
                errorCallback, onBitmapGenerated);
    }

    /**
     * Destroy and clean up any memory.
     */
    public void destroy() {
        if (mCompositor != null) {
            mCompositor.destroy();
            mCompositor = null;
        }
        if (mTabService != null) {
            mTabService.longScreenshotsClosed();
        }
    }

    @VisibleForTesting
    public void setTabServiceAndCompositorForTest(
            LongScreenshotsTabService tabService, LongScreenshotsCompositor compositor) {
        mTabService = tabService;
        mCompositor = compositor;
    }

    private void onCompositorResult(@CompositorStatus int status) {
        mGeneratorCallBack.onCompositorResult(status);
    }

    private void onCaptureResult(@Status int status) {
        // TODO(tgupta): Add metrics logging here.
        mGeneratorCallBack.onCaptureResult(status);
    }
}
