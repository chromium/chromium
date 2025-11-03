// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.os.Handler;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.BrowserControlsState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Handles overscroll on the bottom. */
@NullMarked
public class BottomOverscrollHandler {

    /**
     * Enum for "Android.EdgeToEdge.OverscrollFromBottom.BottomControlsStatus", demonstrate the
     * current status for the bottom browser controls. These values are persisted to logs. Entries
     * should not be renumbered and numeric values should never be reused.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        BottomControlsStatus.HEIGHT_ZERO,
        BottomControlsStatus.HIDDEN,
        BottomControlsStatus.VISIBLE_FULL_HEIGHT,
        BottomControlsStatus.VISIBLE_PARTIAL_HEIGHT,
        BottomControlsStatus.NUM_TOTAL
    })
    @interface BottomControlsStatus {
        /** Controls has a height of 0. */
        int HEIGHT_ZERO = 0;

        /** Controls has height > 0, and it's hidden */
        int HIDDEN = 1;

        /** Controls has height > 0 and is fully visible. */
        int VISIBLE_FULL_HEIGHT = 2;

        /** Controls has height > 0 and is partially visible (e.g. showing its min Height) */
        int VISIBLE_PARTIAL_HEIGHT = 3;

        int NUM_TOTAL = 4;
    }

    private final BrowserControlsVisibilityManager mBrowserControls;
    private final Handler mHandler = new Handler();
    private final Runnable mShowControlsRunnable = this::showControlsTransient;

    private boolean mOverscrollStarted;

    /**
     * Create the instance.
     *
     * @param browserControls {@link BrowserControlsVisibilityManager} instance in the activity.
     */
    public BottomOverscrollHandler(BrowserControlsVisibilityManager browserControls) {
        mBrowserControls = browserControls;
    }

    /**
     * Respond to the start a overscroll.
     *
     * @return Whether the overscroll started.
     */
    public boolean start() {
        recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        mOverscrollStarted = canStartBottomOverscroll();

        RecordHistogram.recordBooleanHistogram(
                "Android.OverscrollFromBottom.CanStart", mOverscrollStarted);
        return mOverscrollStarted;
    }

    private boolean canStartBottomOverscroll() {
        if (mBrowserControls.isVisibilityForced()) {
            return false;
        }

        if (mBrowserControls.getBrowserVisibilityDelegate().get() != BrowserControlsState.BOTH) {
            return false;
        }

        return mBrowserControls.getTopControlOffset() != 0
                || mBrowserControls.getBottomControlOffset() != 0;
    }

    /**
     * Respond to a pull during overscroll.
     *
     * @param yDelta The change in vertical pull distance.
     */
    public void pull(float yDelta) {
        // TODO: Implement
    }

    /**
     * Processes a motion event releasing the finger off the screen.
     *
     * @param allowTrigger Whether the scroll passes the threshold to trigger the feature.
     */
    public void release(boolean allowTrigger) {
        // TODO: Implement
        if (mOverscrollStarted && allowTrigger) {
            mOverscrollStarted = false;
            mHandler.post(mShowControlsRunnable);
        }
    }

    /** Resets a gesture as the result of the successful overscroll or cancellation. */
    public void reset() {
        if (mOverscrollStarted) {
            recordDidTriggerOverscroll(false);
        }
        mOverscrollStarted = false;
        mHandler.removeCallbacks(mShowControlsRunnable);
    }

    private void showControlsTransient() {
        mBrowserControls.getBrowserVisibilityDelegate().showControlsTransient();
        recordDidTriggerOverscroll(true);
    }

    /**
     * Record histogram "Android.OverscrollFromBottom.BottomControlsStatus" based on the current
     * browser controls status.
     */
    @VisibleForTesting
    static void recordEdgeToEdgeOverscrollFromBottom(BrowserControlsStateProvider browserControls) {
        @BottomControlsStatus int sample;
        if (browserControls.getBottomControlsHeight() == 0) {
            sample = BottomControlsStatus.HEIGHT_ZERO;
        } else if (browserControls.getBottomControlOffset() == 0) {
            sample = BottomControlsStatus.VISIBLE_FULL_HEIGHT;
        } else if (browserControls.getBottomControlOffset()
                == browserControls.getBottomControlsHeight()) {
            sample = BottomControlsStatus.HIDDEN;
        } else {
            sample = BottomControlsStatus.VISIBLE_PARTIAL_HEIGHT;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.OverscrollFromBottom.BottomControlsStatus",
                sample,
                BottomControlsStatus.NUM_TOTAL);
    }

    /** Record whether the bottom overscroll is triggered, or canceled. */
    static void recordDidTriggerOverscroll(boolean didTrigger) {
        RecordHistogram.recordBooleanHistogram(
                "Android.OverscrollFromBottom.DidTriggerOverscroll", didTrigger);
    }
}
