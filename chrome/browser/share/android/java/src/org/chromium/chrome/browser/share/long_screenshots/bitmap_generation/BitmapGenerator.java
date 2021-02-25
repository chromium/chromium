// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.NonNull;
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
    private Context mContext;
    private Rect mCaptureRect;
    // Response with a pointer to the skia image
    private PaintPreviewProto mProtoResponse;

    // Compositor delegate responsible for compositing the skia
    private LongScreenshotsCompositor mCompositor;
    private LongScreenshotsTabService mTabService;
    private Tab mTab;

    private static final String DIR_NAME = "long_screenshots_dir";

    protected GeneratorCallBack mGeneratorCallBack;

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
     * @param context An instance of current Android {@link Context}.
     * @param tab The current tab being screen-shotted.
     * @param rect The area of the webpage to capture
     * @param callback Callback to receive updates from the generation.
     */
    public BitmapGenerator(
            Context context, Tab tab, @NonNull Rect rect, GeneratorCallBack callback) {
        mContext = context;
        mTab = tab;
        mCaptureRect = rect;
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
        mTabService.captureTab(mTab, new Rect(0, mCaptureRect.top, 0, mCaptureRect.height()));
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
        return mCompositor.requestBitmap(
                calculateRelativeRect(rect), errorCallback, onBitmapGenerated);
    }

    /**
     * Calculates the bounds passed in relative to the bounds of the capture. Since 6x
     * the viewport size is captured, the composite bounds needs to be adjusted to be relative to
     * the captured page.
     *
     * For example, let's say that the top Y-axis of the capture rectangle is 100 relative to the
     * top of the website. The Y-axis of the composite rectangle is 150 relative to the top of the
     * website. Then the relative top Y-axis to be used for compositing should be 50 where the top
     * is assumed to the top of the capture.
     *
     * @param compositeRect The bounds relative to the webpage
     * @return The bounds relative to the capture.
     */
    private Rect calculateRelativeRect(Rect compositeRect) {
        int startY = compositeRect.top - mCaptureRect.top;
        startY = (startY < mCaptureRect.top) ? mCaptureRect.top : startY;

        return new Rect(0, startY, 0, startY + compositeRect.height());
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
