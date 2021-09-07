// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.CancellationSignal;
import android.view.ScrollCaptureCallback;
import android.view.ScrollCaptureSession;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager.BitmapGeneratorObserver;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

import java.util.function.Consumer;

/**
 * Implementation of ScrollCaptureCallback that provides snapshots of the tab using Paint
 * Previews.
 */
@RequiresApi(api = VERSION_CODES.S)
public class ScrollCaptureCallbackImpl implements ScrollCaptureCallback {
    /**
     * Wrapper class for {@link EntryManager}.
     */
    public static class EntryManagerWrapper {
        EntryManager create(Tab tab) {
            return new EntryManager(tab.getContext(), tab);
        }
    }

    private static final int BITMAP_HEIGHT_THRESHOLD = 20;

    private final EntryManagerWrapper mEntryManagerWrapper;
    private Tab mCurrentTab;
    private RenderCoordinates mRenderCoordinates;
    private EntryManager mEntryManager;

    private Rect mContentArea;
    // Holds the coordinates of the initial visible content with respect to the content area.
    private Rect mInitialRect;
    // Holds the viewport size.
    private Rect mViewportRect;

    public ScrollCaptureCallbackImpl(EntryManagerWrapper entryManagerWrapper) {
        this.mEntryManagerWrapper = entryManagerWrapper;
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureSearch(
            @NonNull CancellationSignal cancellationSignal, @NonNull Consumer<Rect> onReady) {
        assert mCurrentTab != null;
        WebContents webContents = mCurrentTab.getWebContents();
        if (webContents == null || mCurrentTab.isFrozen()) {
            onReady.accept(new Rect());
            return;
        }

        mRenderCoordinates = RenderCoordinates.fromWebContents(webContents);
        mViewportRect = new Rect(0, 0, mRenderCoordinates.getLastFrameViewportWidthPixInt(),
                mRenderCoordinates.getLastFrameViewportHeightPixInt());
        onReady.accept(mViewportRect);
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureStart(@NonNull ScrollCaptureSession session,
            @NonNull CancellationSignal signal, @NonNull Runnable onReady) {
        assert mCurrentTab != null;
        // TODO(crbug.com/1239297): Add support for subscrollers.
        mContentArea =
                new Rect(0, 0, getScaledCoordinate(mRenderCoordinates.getContentWidthPixInt()),
                        getScaledCoordinate(mRenderCoordinates.getContentHeightPixInt()));
        // translate the viewport rect to its coordinates with respect to the content area.
        mInitialRect = new Rect(mViewportRect);
        mInitialRect.offsetTo(0, getScaledCoordinate(mRenderCoordinates.getScrollYPixInt()));
        mEntryManager = mEntryManagerWrapper.create(mCurrentTab);
        mEntryManager.addBitmapGeneratorObserver(new BitmapGeneratorObserver() {
            @Override
            public void onStatusChange(int status) {
                if (status == EntryStatus.CAPTURE_IN_PROGRESS) return;

                mEntryManager.removeBitmapGeneratorObserver(this);
                if (status == EntryStatus.CAPTURE_COMPLETE) {
                    onReady.run();
                } else {
                    signal.cancel();
                }
            }
        });
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureImageRequest(@NonNull ScrollCaptureSession session,
            @NonNull CancellationSignal signal, @NonNull Rect captureArea,
            @NonNull Consumer<Rect> onComplete) {
        // Reposition the captureArea to the content area coordinates.
        captureArea.offset(mInitialRect.left, mInitialRect.top);
        if (!captureArea.intersect(mContentArea)
                || captureArea.height() < BITMAP_HEIGHT_THRESHOLD) {
            onComplete.accept(new Rect());
            return;
        }

        LongScreenshotsEntry entry = mEntryManager.generateEntry(captureArea);
        entry.setListener(status -> {
            if (status == EntryStatus.BITMAP_GENERATION_IN_PROGRESS) return;

            Bitmap bitmap = entry.getBitmap();
            if (status != EntryStatus.BITMAP_GENERATED || bitmap == null) {
                onComplete.accept(new Rect());
                return;
            }

            Rect destRect = new Rect(0, 0, captureArea.width(), captureArea.height());
            Canvas canvas = session.getSurface().lockCanvas(destRect);
            canvas.drawBitmap(bitmap, null, destRect, null);
            session.getSurface().unlockCanvasAndPost(canvas);
            // Translate the captureArea Rect back to its original coordinates.
            captureArea.offset(-mInitialRect.left, -mInitialRect.top);
            onComplete.accept(captureArea);
        });
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureEnd(@NonNull Runnable onReady) {
        mRenderCoordinates = null;
        mEntryManager = null;
        mContentArea = null;
        mInitialRect = null;
        mViewportRect = null;
        onReady.run();
    }

    private int getScaledCoordinate(double value) {
        return (int) Math.floor(value / mRenderCoordinates.getPageScaleFactor());
    }

    void setCurrentTab(Tab tab) {
        mCurrentTab = tab;
    }

    @VisibleForTesting
    Rect getContentAreaForTesting() {
        return mContentArea;
    }

    @VisibleForTesting
    Rect getInitialRectForTesting() {
        return mInitialRect;
    }
}
