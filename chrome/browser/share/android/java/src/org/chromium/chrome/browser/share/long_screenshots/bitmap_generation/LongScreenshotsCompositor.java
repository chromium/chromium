// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegateImpl;
import org.chromium.url.GURL;

/**
 * Compositor for LongScreenshots. Responsible for calling into Freeze-dried tabs to composite the
 * captured webpage.
 */
public class LongScreenshotsCompositor {
    private PlayerCompositorDelegate mDelegate;
    private Callback<Integer> mCompositorCallback;
    private Size mContentSize;
    private Point mScrollOffset;

    private static PlayerCompositorDelegate.Factory sCompositorDelegateFactory =
            new CompositorDelegateFactory();

    /**
     * Creates a new {@link LongScreenshotsCompositor}.
     *
     * @param url The URL for which the content should be composited for.
     * @param nativePaintPreviewServiceProvider The native paint preview service.
     * @param directoryKey The key for the directory storing the data.
     * @param nativeCaptureResultPtr A pointer to a native paint_preview::CaptureResult.
     */
    public LongScreenshotsCompositor(
            GURL url,
            NativePaintPreviewServiceProvider nativePaintPreviewServiceProvider,
            String directoryKey,
            long nativeCaptureResultPtr,
            Callback<Integer> compositorCallback) {
        mCompositorCallback = compositorCallback;

        mDelegate =
                getCompositorDelegateFactory()
                        .createForCaptureResult(
                                nativePaintPreviewServiceProvider,
                                nativeCaptureResultPtr,
                                url,
                                directoryKey,
                                true,
                                this::onCompositorReady,
                                this::onCompositorError);
    }

    /** Called when the compositor cannot be successfully initialized. */
    @VisibleForTesting
    protected void onCompositorError(@CompositorStatus int status) {
        mCompositorCallback.onResult(status);
    }

    /**
     * Called by {@link PlayerCompositorDelegateImpl} when the compositor is initialized. This
     * method initializes a sub-component for each frame and adds the view for the root frame to
     * {@link #mHostView}.
     */
    @VisibleForTesting
    protected void onCompositorReady(
            UnguessableToken rootFrameGuid,
            UnguessableToken[] frameGuids,
            int[] frameContentSize,
            int[] scrollOffsets,
            int[] subFramesCount,
            UnguessableToken[] subFrameGuids,
            int[] subFrameClipRects,
            float pageScaleFactor,
            long nativeAxTree) {
        mContentSize = getMainFrameValues(frameContentSize);
        Size offsetSize = getMainFrameValues(scrollOffsets);
        mScrollOffset = new Point(offsetSize.getWidth(), offsetSize.getHeight());
        mCompositorCallback.onResult(CompositorStatus.OK);
    }

    private Size getMainFrameValues(int[] arr) {
        if (arr != null && arr.length >= 2) {
            return new Size(arr[0], arr[1]);
        }
        return new Size(0, 0);
    }

    /**
     * Requests the bitmap.
     *
     * @param rect The bounds of the capture to convert to a bitmap.
     * @param errorCallback Called when an error is encountered.
     * @param bitmapCallback Called when a bitmap was successfully generated.
     * @return id for the request.
     */
    public int requestBitmap(
            Rect rect, float scaleFactor, Runnable errorCallback, Callback<Bitmap> bitmapCallback) {
        // Check that the rect is within the bounds.
        return mDelegate.requestBitmap(rect, scaleFactor, bitmapCallback, errorCallback);
    }

    public void destroy() {
        if (mDelegate != null) {
            mDelegate.destroy();
            mDelegate = null;
        }
    }

    static class CompositorDelegateFactory implements PlayerCompositorDelegate.Factory {
        @Override
        public PlayerCompositorDelegate create(
                NativePaintPreviewServiceProvider service,
                @NonNull GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return new PlayerCompositorDelegateImpl(
                    service,
                    0,
                    url,
                    directoryKey,
                    mainFrameMode,
                    compositorListener,
                    compositorErrorCallback);
        }

        @Override
        public PlayerCompositorDelegate createForCaptureResult(
                NativePaintPreviewServiceProvider service,
                long nativeCaptureResultPtr,
                @NonNull GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return new PlayerCompositorDelegateImpl(
                    service,
                    nativeCaptureResultPtr,
                    url,
                    directoryKey,
                    mainFrameMode,
                    compositorListener,
                    compositorErrorCallback);
        }
    }

    private PlayerCompositorDelegate.Factory getCompositorDelegateFactory() {
        return sCompositorDelegateFactory;
    }

    public @Nullable Size getContentSize() {
        return mContentSize;
    }

    public @Nullable Point getScrollOffset() {
        return mScrollOffset;
    }

    public static void overrideCompositorDelegateFactoryForTesting(
            PlayerCompositorDelegate.Factory factory) {
        sCompositorDelegateFactory = factory; // IN-TEST
    }
}
