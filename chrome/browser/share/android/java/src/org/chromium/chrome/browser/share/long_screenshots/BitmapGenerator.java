// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.url.GURL;

/**
 * Class responsible for processing the initial request, calling {@link LongScreenshotsTab} service
 * to capture the webpage, then using {@link LongScreenshotsCompositor} to composite the bitmap.
 * Callers of this class should supply a GeneratorCallback to receive the final bitmap or any errors
 * along the way.
 */
public class BitmapGenerator implements LongScreenshotsTabService.CaptureProcessor {
    private Context mContext;
    private Rect mRect;
    // Response with a pointer to the skia image
    private PaintPreviewProto mProtoResponse;

    // Compositor delegate responsible for compositing the skia
    private LongScreenshotsCompositor mCompositor;
    private LongScreenshotsTabService mLongScreenshotsTabService;
    private Tab mTab;

    private static final String DIR_NAME = "long_screenshots_dir";

    protected GeneratorCallBack mGeneratorCallback;

    /**
     * Users of the {@link LongScreenshotsEntry} class have to implement and pass this interface in
     * the constructor.
     */
    public interface GeneratorCallBack {
        /**
         * Called when the compositor cannot be successfully initialized.
         */
        void onCompositorError(@CompositorStatus int status);

        /**
         * Called when the bitmap has been successfully generated.
         */
        void onBitmapGenerated(Bitmap bitmap);

        /**
         * Called when the capture failed.
         */
        void onCaptureError(@Status int status);
    }

    /**
     * @param context An instance of current Android {@link Context}.
     * @param tab The current tab being screen-shotted.
     * @param rect The area of the webpage to capture
     * @param callback Callback to receive updates from the generation.
     */
    public BitmapGenerator(
            Context context, Tab tab, @NonNull Rect rect, GeneratorCallBack callback) {
        mContext = context;
        mTab = tab;
        mGeneratorCallback = callback;
        mRect = rect;

        mLongScreenshotsTabService = LongScreenshotsTabServiceFactory.getServiceInstance();
        mLongScreenshotsTabService.setCaptureProcessor(this);
    }

    /**
     * Starts the capture of the screenshot.
     */
    public void captureScreenshot() {
        mLongScreenshotsTabService.captureTab(mTab, mRect);
    }

    /**
     * Called from native after the tab has been captured.
     *
     * @param response
     * @param status
     */
    @Override
    public void processCapturedTab(PaintPreviewProto response, @Status int status) {
        if (status != Status.OK) {
            mGeneratorCallback.onCaptureError(status);
        } else {
            mCompositor = new LongScreenshotsCompositor(new GURL(response.getMetadata().getUrl()),
                    mLongScreenshotsTabService, DIR_NAME, response, mRect,
                    mGeneratorCallback::onBitmapGenerated, mGeneratorCallback::onCompositorError);
        }
    }

    /**
     * Called after the bitmap has been composited and can be shown to the user.
     *
     * @param result Bitmap to display in the dialog.
     */
    private void onBitmapResult(Bitmap result) {
        mGeneratorCallback.onBitmapGenerated(result);
    }

    /**
     * Destroy and clean up any memory.
     */
    public void destroy() {
        if (mCompositor != null) {
            mCompositor.destroy();
            mCompositor = null;
        }
    }
}
