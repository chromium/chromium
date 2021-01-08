// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

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
    private Callback<Bitmap> mBitmapCallback;
    private Rect mRect;

    private static PlayerCompositorDelegate.Factory sCompositorDelegateFactory =
            new CompositorDelegateFactory();

    /**
     * Creates a new {@link LongScreenshotsCompositor}.
     *
     * @param url The URL for which the content should be composited for.
     * @param nativePaintPreviewServiceProvider The native paint preview service.
     * @param directoryKey The key for the directory storing the data.
     * @param rect The area of the captured webpage that should be composited.
     * @param response The proto with the address of the captured bitmap.
     * @param bitmapCallback Callback to process the composited bitmap.
     */
    public LongScreenshotsCompositor(GURL url,
            NativePaintPreviewServiceProvider nativePaintPreviewServiceProvider,
            String directoryKey, PaintPreviewProto response, Rect rect,
            Callback<Bitmap> bitmapCallback) {
        mBitmapCallback = bitmapCallback;
        mRect = rect;

        // TODO(tgupta): Look into warmupCompositor
        mDelegate = getCompositorDelegateFactory().createForProto(nativePaintPreviewServiceProvider,
                response, url, directoryKey, true, this::onCompositorReady,
                this::onCompositorError);
    }

    /**
     * Called when the compositor cannot be successfully initialized.
     */
    private void onCompositorError(@CompositorStatus int status) {
        // do nothing for now
        // TODO(tgupta): Figure out what to display to the user at this point.
    }

    /**
     * Called by {@link PlayerCompositorDelegateImpl} when the compositor is initialized. This
     * method initializes a sub-component for each frame and adds the view for the root frame to
     * {@link #mHostView}.
     */
    @VisibleForTesting
    protected void onCompositorReady(UnguessableToken rootFrameGuid, UnguessableToken[] frameGuids,
            int[] frameContentSize, int[] scrollOffsets, int[] subFramesCount,
            UnguessableToken[] subFrameGuids, int[] subFrameClipRects) {
        // TODO(tgupta): Keep track of the returned id.
        mDelegate.requestBitmap(mRect, 1, mBitmapCallback, this::onError);
    }

    /**
     * Called when there was an error compositing the bitmap.
     */
    public void onError() {
        // do nothing for now
        // TODO(tgupta): Figure out what to display to the user at this point.
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
