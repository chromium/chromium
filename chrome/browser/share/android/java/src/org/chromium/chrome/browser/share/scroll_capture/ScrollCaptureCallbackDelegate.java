// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.CancellationSignal;
import android.os.SystemClock;
import android.util.Size;
import android.view.Surface;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager.BitmapGeneratorObserver;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;

/** An delegate to provide an Android API level independent implementation Scroll Capture. */
public class ScrollCaptureCallbackDelegate {
    private static final int BITMAP_HEIGHT_THRESHOLD = 20;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        BitmapGeneratorStatus.CAPTURE_COMPLETE,
        BitmapGeneratorStatus.INSUFFICIENT_MEMORY,
        BitmapGeneratorStatus.GENERATION_ERROR
    })
    private @interface BitmapGeneratorStatus {
        int CAPTURE_COMPLETE = 0;
        int INSUFFICIENT_MEMORY = 1;
        int GENERATION_ERROR = 2;
        int COUNT = 3;
    }

    /** Wrapper class for {@link EntryManager}. */
    public static class EntryManagerWrapper {
        EntryManager create(Tab tab) {
            return new EntryManager(tab.getContext(), tab, /* inMemory= */ true);
        }
    }

    private final EntryManagerWrapper mEntryManagerWrapper;
    private Tab mCurrentTab;
    private EntryManager mEntryManager;

    private Rect mContentArea;
    // Holds the viewport size.
    private Rect mViewportRect;

    private int mInitialYOffset;
    private float mMinPageScaleFactor;

    private long mCaptureStartTime;

    public ScrollCaptureCallbackDelegate(EntryManagerWrapper entryManagerWrapper) {
        mEntryManagerWrapper = entryManagerWrapper;
    }

    /** See {@link ScrollCaptureCallback#onScrollCaptureSearch}. */
    public Rect onScrollCaptureSearch(@NonNull CancellationSignal cancellationSignal) {
        assert mCurrentTab != null;
        WebContents webContents = mCurrentTab.getWebContents();
        View view = mCurrentTab.getView();
        if (view == null || webContents == null || mCurrentTab.isFrozen()) {
            return new Rect();
        }

        RenderCoordinates renderCoordinates = RenderCoordinates.fromWebContents(webContents);
        // getLastFrameViewport{Width|Height}PixInt() always reports in physical pixels no matter
        // the scale factor.
        mViewportRect =
                new Rect(
                        0,
                        0,
                        renderCoordinates.getLastFrameViewportWidthPixInt(),
                        renderCoordinates.getLastFrameViewportHeightPixInt());
        mMinPageScaleFactor = renderCoordinates.getMinPageScaleFactor();
        return mViewportRect;
    }

    /** See {@link ScrollCaptureCallback#onScrollCaptureStart}. */
    public void onScrollCaptureStart(
            @NonNull CancellationSignal signal, @NonNull Runnable onReady) {
        assert mCurrentTab != null;

        mCaptureStartTime = SystemClock.elapsedRealtime();
        mEntryManager = mEntryManagerWrapper.create(mCurrentTab);
        mEntryManager.addBitmapGeneratorObserver(
                new BitmapGeneratorObserver() {
                    @Override
                    public void onStatusChange(int status) {
                        if (status == EntryStatus.CAPTURE_IN_PROGRESS) return;

                        // Abort if BitmapGenerator is not initialized successfully.
                        if (status != EntryStatus.CAPTURE_COMPLETE) {
                            mEntryManager.removeBitmapGeneratorObserver(this);
                            mEntryManager.destroy();
                            signal.cancel();
                            // The compositor won't be started so stop the pre-warmed compositor.
                            PaintPreviewCompositorUtils.stopWarmCompositor();
                            if (status == EntryStatus.INSUFFICIENT_MEMORY) {
                                logBitmapGeneratorStatus(BitmapGeneratorStatus.INSUFFICIENT_MEMORY);
                            } else {
                                logBitmapGeneratorStatus(BitmapGeneratorStatus.GENERATION_ERROR);
                            }
                        }
                    }

                    @Override
                    public void onCompositorReady(Size contentSize, Point scrollOffset) {
                        mEntryManager.removeBitmapGeneratorObserver(this);
                        if (contentSize.getWidth() == 0 || contentSize.getHeight() == 0) {
                            mEntryManager.destroy();
                            signal.cancel();
                            logBitmapGeneratorStatus(BitmapGeneratorStatus.GENERATION_ERROR);
                            return;
                        }

                        // Store the content area and Y offset in physical pixel coordinates
                        int width = (int) Math.floor(contentSize.getWidth() * mMinPageScaleFactor);
                        int height =
                                (int) Math.floor(contentSize.getHeight() * mMinPageScaleFactor);
                        mContentArea = new Rect(0, 0, width, height);
                        mInitialYOffset = (int) Math.floor(scrollOffset.y * mMinPageScaleFactor);
                        logBitmapGeneratorStatus(BitmapGeneratorStatus.CAPTURE_COMPLETE);
                        onReady.run();
                    }
                });
        PaintPreviewCompositorUtils.warmupCompositor();
    }

    /** See {@link ScrollCaptureCallback#onScrollCaptureImageRequest}. */
    public void onScrollCaptureImageRequest(
            @NonNull Surface surface,
            @NonNull CancellationSignal signal,
            @NonNull Rect captureArea,
            Callback<Rect> onComplete) {
        // Reposition the captureArea to the content area coordinates.
        captureArea.offset(0, mInitialYOffset);
        if (!captureArea.intersect(mContentArea)
                || captureArea.height() < BITMAP_HEIGHT_THRESHOLD) {
            onComplete.onResult(new Rect());
            return;
        }

        LongScreenshotsEntry entry = mEntryManager.generateEntry(captureArea);
        entry.setListener(
                status -> {
                    if (status == EntryStatus.BITMAP_GENERATION_IN_PROGRESS) return;

                    Bitmap bitmap = entry.getBitmap();
                    if (status != EntryStatus.BITMAP_GENERATED || bitmap == null) {
                        onComplete.onResult(new Rect());
                        return;
                    }

                    Rect destRect = new Rect(0, 0, captureArea.width(), captureArea.height());
                    Canvas canvas = surface.lockCanvas(destRect);
                    canvas.drawColor(Color.WHITE);
                    canvas.drawBitmap(bitmap, null, destRect, null);
                    surface.unlockCanvasAndPost(canvas);
                    // Translate the captureArea Rect back to its original coordinates.
                    captureArea.offset(0, -mInitialYOffset);
                    onComplete.onResult(captureArea);
                    return;
                });
    }

    /** See {@link ScrollCaptureCallback#onScrollCaptureEnd}. */
    public void onScrollCaptureEnd(@NonNull Runnable onReady) {
        PaintPreviewCompositorUtils.stopWarmCompositor();
        if (mEntryManager != null) {
            mEntryManager.destroy();
            mEntryManager = null;
        }
        if (mCaptureStartTime != 0) {
            RecordHistogram.recordTimesHistogram(
                    "Sharing.ScrollCapture.SuccessfulCaptureDuration",
                    SystemClock.elapsedRealtime() - mCaptureStartTime);
        }
        mCaptureStartTime = 0;
        mContentArea = null;
        mViewportRect = null;
        mInitialYOffset = 0;
        mMinPageScaleFactor = 1f;
        onReady.run();
    }

    void setCurrentTab(Tab tab) {
        mCurrentTab = tab;
    }

    private void logBitmapGeneratorStatus(@BitmapGeneratorStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.ScrollCapture.BitmapGeneratorStatus", status, BitmapGeneratorStatus.COUNT);
    }

    Rect getContentAreaForTesting() {
        return mContentArea;
    }

    int getInitialYOffsetForTesting() {
        return mInitialYOffset;
    }
}
