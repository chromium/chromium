// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegateImpl;
import org.chromium.content_public.browser.RenderCoordinates;

/**
 * Compositor for LongScreenshots. Responsible for calling into Freeze-dried tabs to composite
 * the captured webpage.
 */
public class LongScreenshotsCompositor {
    private Context mContext;
    private PlayerCompositorDelegate mDelegate;
    private Callback<Bitmap> mBitmapCallback;
    private Tab mTab;

    private static final int CLIP_HEIGHT = 1000;

    /**
     * Creates a new {@link LongScreenshotsCompositor}.
     *
     * @param tab The tab for which the content should be captured for.
     * @param context An instance of current Android {@link Context}.
     * @param nativePaintPreviewServiceProvider The native paint preview service.
     * @param directoryKey The key for the directory storing the data.
     * @param response The proto with the address of the captured bitmap.
     * @param bitmapCallback Callback to process the composited bitmap.
     */
    public LongScreenshotsCompositor(Tab tab, Context context,
            NativePaintPreviewServiceProvider nativePaintPreviewServiceProvider,
            String directoryKey, PaintPreviewProto response, Callback<Bitmap> bitmapCallback) {
        mTab = tab;
        mContext = context;
        mBitmapCallback = bitmapCallback;

        // TODO(tgupta): Look into warmupCompositor
        // TODO(tgupta): Investigate why the PlayerCompositorDelegateFactory is the preferred
        // way to construct a PlayerCompositorDelegate.
        mDelegate = new PlayerCompositorDelegateImpl(nativePaintPreviewServiceProvider, response,
                mTab.getUrl(), directoryKey, true, this::onCompositorReady,
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
    private void onCompositorReady(UnguessableToken rootFrameGuid, UnguessableToken[] frameGuids,
            int[] frameContentSize, int[] scrollOffsets, int[] subFramesCount,
            UnguessableToken[] subFrameGuids, int[] subFrameClipRects) {
        RenderCoordinates coords = RenderCoordinates.fromWebContents(mTab.getWebContents());

        int startY = coords.getScrollYPixInt() / coords.getPageScaleFactorInt();
        int endX = coords.getContentWidthPixInt() / coords.getPageScaleFactorInt();
        int endY = startY + (CLIP_HEIGHT * coords.getPageScaleFactorInt());
        Rect rect = new Rect(0, startY, endX, endY);
        mDelegate.requestBitmap(rect, 1, mBitmapCallback, this::onError);
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
}
