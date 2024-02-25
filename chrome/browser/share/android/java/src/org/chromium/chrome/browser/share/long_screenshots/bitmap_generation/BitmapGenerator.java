// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.url.GURL;

/**
 * Class responsible for processing the initial request, calling {@link LongScreenshotsTab} service
 * to capture the webpage, then using {@link LongScreenshotsCompositor} to composite the bitmap.
 * Callers of this class should supply a GeneratorCallback to receive status updates.
 */
public class BitmapGenerator implements LongScreenshotsTabService.CaptureProcessor {
    // Compositor delegate responsible for compositing the skia
    private LongScreenshotsCompositor mCompositor;
    private LongScreenshotsTabService mTabService;
    private Tab mTab;

    private static final String DIR_NAME = "long_screenshots_dir";

    protected GeneratorCallBack mGeneratorCallBack;
    private CompositorFactory mCompositorFactory;
    private ScreenshotBoundsManager mBoundsManager;
    private float mScaleFactor;

    /**
     * Users of the {@link LongScreenshotsEntry} class have to implement and pass this interface in
     * the constructor.
     */
    public interface GeneratorCallBack {
        /** Called when the compositor cannot be successfully initialized. */
        void onCompositorResult(@CompositorStatus int status);

        /** Called when the capture is complete. */
        void onCaptureResult(@Status int status);
    }

    /** Tests can override the {@link CompositorFactory} to inject a compositor. */
    public interface CompositorFactory {
        /** Identical interface to {@link LongScreenshotsCompositor} constructor. */
        LongScreenshotsCompositor create(
                GURL url,
                LongScreenshotsTabService tabService,
                String directoryName,
                long nativeCaptureResultPtr,
                Callback<Integer> callback);
    }

    /**
     * @param tab The current tab being screen-shotted.
     * @param boundsManager The bounds manager of the page to determine capture regions.
     * @param callback Callback to receive updates from the generation.
     */
    public BitmapGenerator(
            Tab tab, ScreenshotBoundsManager boundsManager, GeneratorCallBack callback) {
        mTab = tab;
        mBoundsManager = boundsManager;
        mGeneratorCallBack = callback;
        mCompositorFactory = LongScreenshotsCompositor::new;
    }

    /**
     * @param compositorFactory The compositor factory to use.
     */
    public void setCompositorFactoryForTesting(CompositorFactory compositorFactory) {
        mCompositorFactory = compositorFactory;
    }

    /**
     * Starts the capture of the screenshot.
     * @param inMemory Capture the contents of the tab in memory rather than using temporary files.
     */
    public void captureTab(boolean inMemory) {
        if (mTabService == null) {
            mTabService = LongScreenshotsTabServiceFactory.getServiceInstance();
        }
        mTabService.setCaptureProcessor(this);
        mTabService.captureTab(mTab, mBoundsManager.getCaptureBounds(), inMemory);
        mScaleFactor = 0f;
    }

    /**
     * Called from native after the tab has been captured. If status is OK, then calls the
     * compositor on the response. Otherwise, calls the GeneratorCallback with the status.
     *
     * @param nativeCaptureResultPtr Response with details about the capture.
     * @param status Status of the capture.
     */
    @Override
    public void processCapturedTab(long nativeCaptureResultPtr, @Status int status) {
        if (status == Status.OK && mCompositor == null) {
            mCompositor =
                    mCompositorFactory.create(
                            GURL.emptyGURL(),
                            mTabService,
                            DIR_NAME,
                            nativeCaptureResultPtr,
                            this::onCompositorResult);
            // Don't call {@link #onCaptureResult()} CAPTURE_COMPLETE will be propagated after
            // compositor initialization.
        } else {
            mTabService.releaseNativeCaptureResultPtr(nativeCaptureResultPtr);
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
        if (mScaleFactor == 0f) {
            mScaleFactor = mBoundsManager.getBitmapScaleFactor();
        }
        return mCompositor.requestBitmap(rect, mScaleFactor, errorCallback, onBitmapGenerated);
    }

    /** Destroy and clean up any memory. */
    public void destroy() {
        if (mCompositor != null) {
            mCompositor.destroy();
            mCompositor = null;
        }
        if (mTabService != null) {
            mTabService.longScreenshotsClosed();
        }
    }

    public @Nullable Size getContentSize() {
        if (mCompositor == null) return null;

        return mCompositor.getContentSize();
    }

    public @Nullable Point getScrollOffset() {
        if (mCompositor == null) return null;

        return mCompositor.getScrollOffset();
    }

    public void setTabServiceAndCompositorForTest(
            LongScreenshotsTabService tabService, LongScreenshotsCompositor compositor) {
        mTabService = tabService;
        mCompositor = compositor;
    }

    private void onCompositorResult(@CompositorStatus int status) {
        if (status == CompositorStatus.OK) {
            mBoundsManager.setCompositedSize(mCompositor.getContentSize());
            mBoundsManager.setCompositedScrollOffset(mCompositor.getScrollOffset());
        }
        mGeneratorCallBack.onCompositorResult(status);
    }

    private void onCaptureResult(@Status int status) {
        mGeneratorCallBack.onCaptureResult(status);
    }
}
