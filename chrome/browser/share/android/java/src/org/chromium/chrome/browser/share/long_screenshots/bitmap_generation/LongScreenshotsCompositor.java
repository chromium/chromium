// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
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

    private static PlayerCompositorDelegate.Factory sCompositorDelegateFactory =
            new CompositorDelegateFactory();

    /**
     * Creates a new {@link LongScreenshotsCompositor}.
     *
     * @param url The URL for which the content should be composited for.
     * @param nativePaintPreviewServiceProvider The native paint preview service.
     * @param directoryKey The key for the directory storing the data.
     * @param response The proto with the address of the captured bitmap.
     */
    public LongScreenshotsCompositor(GURL url,
            NativePaintPreviewServiceProvider nativePaintPreviewServiceProvider,
            String directoryKey, PaintPreviewProto response, Callback<Integer> compositorCallback) {
        mCompositorCallback = compositorCallback;

        mDelegate = getCompositorDelegateFactory().createForProto(nativePaintPreviewServiceProvider,
                response, url, directoryKey, true, this::onCompositorReady,
                this::onCompositorError);
    }

    /**
     * Called when the compositor cannot be successfully initialized.
     */
    private void onCompositorError(@CompositorStatus int status) {
        mCompositorCallback.onResult(status);
    }

    /**
     * Called by {@link PlayerCompositorDelegateImpl} when the compositor is initialized. This
     * method initializes a sub-component for each frame and adds the view for the root frame to
     * {@link #mHostView}.
     */
    @VisibleForTesting
    protected void onCompositorReady(UnguessableToken rootFrameGuid, UnguessableToken[] frameGuids,
            int[] frameContentSize, int[] scrollOffsets, int[] subFramesCount,
            UnguessableToken[] subFrameGuids, int[] subFrameClipRects, long nativeAxTree) {
        mCompositorCallback.onResult(CompositorStatus.OK);
    }

    /**
     * Requests the bitmap.
     *
     * @param rect The bounds of the capture to convert to a bitmap.
     * @param errorCallback Called when an error is encountered.
     * @param bitmapCallback Called when a bitmap was successfully generated.
     * @return id for the request.
     */
    public int requestBitmap(Rect rect, Runnable errorCallback, Callback<Bitmap> bitmapCallback) {
        // Check that the rect is within the bounds.
        return mDelegate.requestBitmap(rect, 1, bitmapCallback, errorCallback);
    }

    public void destroy() {
        if (mDelegate != null) {
            mDelegate.destroy();
            mDelegate = null;
        }
    }

    static class CompositorDelegateFactory implements PlayerCompositorDelegate.Factory {
        @Override
        public PlayerCompositorDelegate create(NativePaintPreviewServiceProvider service,
                @NonNull GURL url, String directoryKey, boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return new PlayerCompositorDelegateImpl(service, null, url, directoryKey, mainFrameMode,
                    compositorListener, compositorErrorCallback);
        }

        @Override
        public PlayerCompositorDelegate createForProto(NativePaintPreviewServiceProvider service,
                @Nullable PaintPreviewProto proto, @NonNull GURL url, String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return new PlayerCompositorDelegateImpl(service, proto, url, directoryKey,
                    mainFrameMode, compositorListener, compositorErrorCallback);
        }
    }

    private PlayerCompositorDelegate.Factory getCompositorDelegateFactory() {
        return sCompositorDelegateFactory;
    }

    @VisibleForTesting
    public static void overrideCompositorDelegateFactoryForTesting(
            PlayerCompositorDelegate.Factory factory) {
        sCompositorDelegateFactory = factory; // IN-TEST
    }
}
