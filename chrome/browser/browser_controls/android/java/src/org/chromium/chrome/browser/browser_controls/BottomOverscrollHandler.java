// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.os.Handler;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.cc.input.BrowserControlsState;

/** Handles overscroll on the bottom. */
@NullMarked
public class BottomOverscrollHandler {

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

    /** Record whether the bottom overscroll is triggered, or canceled. */
    static void recordDidTriggerOverscroll(boolean didTrigger) {
        RecordHistogram.recordBooleanHistogram(
                "Android.OverscrollFromBottom.DidTriggerOverscroll", didTrigger);
    }
}
