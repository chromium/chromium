// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.CancellationSignal;
import android.os.SystemClock;
import android.util.Size;
import android.view.ScrollCaptureCallback;
import android.view.ScrollCaptureSession;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
    private static final String IN_MEMORY_CAPTURE = "in_memory_capture";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({BitmapGeneratorStatus.CAPTURE_COMPLETE, BitmapGeneratorStatus.INSUFFICIENT_MEMORY,
            BitmapGeneratorStatus.GENERATION_ERROR})
    private @interface BitmapGeneratorStatus {
        int CAPTURE_COMPLETE = 0;
        int INSUFFICIENT_MEMORY = 1;
        int GENERATION_ERROR = 2;
        int COUNT = 3;
    }

    /**
     * Wrapper class for {@link EntryManager}.
     */
    public static class EntryManagerWrapper {
        EntryManager create(Tab tab) {
            return new EntryManager(tab.getContext(), tab,
                    ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                            ChromeFeatureList.SCROLL_CAPTURE, IN_MEMORY_CAPTURE, false));
        }
    }

    private static final int BITMAP_HEIGHT_THRESHOLD = 20;

    private final EntryManagerWrapper mEntryManagerWrapper;
    private Tab mCurrentTab;
    private EntryManager mEntryManager;

    private Rect mContentArea;
    // Holds the viewport size.
    private Rect mViewportRect;
    private int mInitialYOffset;
    private float mMinPageScaleFactor;

    private long mCaptureStartTime;

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

        RenderCoordinates renderCoordinates = RenderCoordinates.fromWebContents(webContents);
        // getLastFrameViewport{Width|Height}PixInt() always reports in physical pixels no matter
        // the scale factor.
        mViewportRect = new Rect(0, 0, renderCoordinates.getLastFrameViewportWidthPixInt(),
                renderCoordinates.getLastFrameViewportHeightPixInt());
        mMinPageScaleFactor = renderCoordinates.getMinPageScaleFactor();
        onReady.accept(mViewportRect);
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureStart(@NonNull ScrollCaptureSession session,
            @NonNull CancellationSignal signal, @NonNull Runnable onReady) {
        assert mCurrentTab != null;

        mCaptureStartTime = SystemClock.elapsedRealtime();
        mEntryManager = mEntryManagerWrapper.create(mCurrentTab);
        mEntryManager.addBitmapGeneratorObserver(new BitmapGeneratorObserver() {
            @Override
            public void onStatusChange(int status) {
                if (status == EntryStatus.CAPTURE_IN_PROGRESS) return;

                // Abort if BitmapGenerator is not initialized successfully.
                if (status != EntryStatus.CAPTURE_COMPLETE) {
                    mEntryManager.removeBitmapGeneratorObserver(this);
                    mEntryManager.destroy();
                    signal.cancel();
                }
            }

            @Override
            public void onCompositorReady(Size contentSize, Size scrollOffset) {
                mEntryManager.removeBitmapGeneratorObserver(this);
                if (contentSize.getWidth() == 0 || contentSize.getHeight() == 0) {
                    mEntryManager.destroy();
                    signal.cancel();
                    logBitmapGeneratorStatus(BitmapGeneratorStatus.GENERATION_ERROR);
                    return;
                }

                // Store the content area and Y offset in physical pixel coordinates
                mContentArea = new Rect(0, 0,
                        (int) Math.floor(contentSize.getWidth() * mMinPageScaleFactor),
                        (int) Math.floor(contentSize.getHeight() * mMinPageScaleFactor));
                mInitialYOffset = (int) Math.floor(scrollOffset.getHeight() * mMinPageScaleFactor);
                logBitmapGeneratorStatus(BitmapGeneratorStatus.CAPTURE_COMPLETE);
                onReady.run();
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
        captureArea.offset(0, mInitialYOffset);
        if (!captureArea.intersect(mContentArea)
                || captureArea.height() < BITMAP_HEIGHT_THRESHOLD) {
            onComplete.accept(new Rect());
            return;
        }

        LongScreenshotsEntry entry = mEntryManager.generateEntry(captureArea, true);
        entry.setListener(status -> {
            if (status == EntryStatus.BITMAP_GENERATION_IN_PROGRESS) return;

            Bitmap bitmap = entry.getBitmap();
            if (status != EntryStatus.BITMAP_GENERATED || bitmap == null) {
                onComplete.accept(new Rect());
                return;
            }

            Rect destRect = new Rect(0, 0, captureArea.width(), captureArea.height());
            Canvas canvas = session.getSurface().lockCanvas(destRect);
            canvas.drawColor(Color.WHITE);
            canvas.drawBitmap(bitmap, null, destRect, null);
            session.getSurface().unlockCanvasAndPost(canvas);
            // Translate the captureArea Rect back to its original coordinates.
            captureArea.offset(0, -mInitialYOffset);
            onComplete.accept(captureArea);
        });
    }

    @Override
    // TODO(crbug.com/1231201): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureEnd(@NonNull Runnable onReady) {
        if (mEntryManager != null) {
            mEntryManager.destroy();
            mEntryManager = null;
        }
        if (mCaptureStartTime != 0) {
            RecordHistogram.recordTimesHistogram("Sharing.ScrollCapture.SuccessfulCaptureDuration",
                    SystemClock.elapsedRealtime() - mCaptureStartTime);
        }
        mCaptureStartTime = 0;
        mContentArea = null;
        mViewportRect = null;
        mInitialYOffset = 0;
        mMinPageScaleFactor = 1;
        onReady.run();
    }

    void setCurrentTab(Tab tab) {
        mCurrentTab = tab;
    }

    private void logBitmapGeneratorStatus(@BitmapGeneratorStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.ScrollCapture.BitmapGeneratorStatus", status, BitmapGeneratorStatus.COUNT);
    }

    @VisibleForTesting
    Rect getContentAreaForTesting() {
        return mContentArea;
    }

    @VisibleForTesting
    int getInitialYOffsetForTesting() {
        return mInitialYOffset;
    }
}
