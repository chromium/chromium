// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

/**
 * Metrics util class for ephemeral tab.
 */
public class EphemeralTabMetrics {
    /** Remembers whether the panel was opened to the peeking state. */
    private boolean mDidRecordFirstPeek;

    /** The timestamp when the panel entered the peeking state for the first time. */
    private long mPanelPeekedNanoseconds;

    /** Remembers whether the panel was opened fully. */
    private boolean mDidRecordFirstOpen;

    /** The timestamp when the panel entered the opened state for the first time. */
    private long mPanelOpenedNanoseconds;

    /** Whether the panel is in any visible state. */
    private boolean mIsVisible;

    /** Whether the panel was opened beyond peeking state. */
    private boolean mIsViewed;

    /** Records metrics for the peeked panel state. */
    public void recordMetricsForPeeked() {
        mIsVisible = true;
        startPeekTimer();
        // Could be returning to Peek from Open.
        finishOpenTimer();
    }

    /** Records metrics when the panel has gone beyond peek state. */
    public void recordMetricsForViewed() {
        mIsViewed = true;
        finishPeekTimer();
    }

    /** Records metrics when the panel has been fully opened. */
    public void recordMetricsForOpened() {
        mIsVisible = true;
        startOpenTimer();
        finishPeekTimer();
    }

    /** Records metrics when the panel has been closed. */
    public void recordMetricsForClosed(@StateChangeReason int stateChangeReason) {
        if (!mIsVisible) return;

        finishPeekTimer();
        finishOpenTimer();
        RecordHistogram.recordBooleanHistogram("EphemeralTab.CtrPeek", mIsViewed);
        RecordHistogram.recordBooleanHistogram("EphemeralTab.Ctr", mDidRecordFirstOpen);
        RecordHistogram.recordEnumeratedHistogram("EphemeralTab.BottomSheet.CloseReason",
                stateChangeReason, StateChangeReason.MAX_VALUE + 1);
        reset();
    }

    /** Records a user action that promotes the ephemeral tab to a full tab. */
    public void recordOpenInNewTab() {
        recordMetricsForClosed(StateChangeReason.PROMOTE_TAB);
        RecordUserAction.record("EphemeralTab.OpenInNewTab");
    }

    /** Records a user action that navigates to a new link on the ephemeral tab. */
    public void recordNavigateLink() {
        RecordUserAction.record("EphemeralTab.NavigateLink");
    }

    /** Resets all internal state including metrics and timers. */
    private void reset() {
        mDidRecordFirstPeek = false;
        mPanelPeekedNanoseconds = 0;
        mDidRecordFirstOpen = false;
        mPanelOpenedNanoseconds = 0;
        mIsVisible = false;
        mIsViewed = false;
    }

    /** Starts timing the peek state if it's not already been started. */
    private void startPeekTimer() {
        if (mPanelPeekedNanoseconds == 0) mPanelPeekedNanoseconds = System.nanoTime();
    }

    /** Finishes timing metrics for the first peek state, unless that has already been done. */
    private void finishPeekTimer() {
        if (!mDidRecordFirstPeek && mPanelPeekedNanoseconds != 0) {
            mDidRecordFirstPeek = true;
            long durationPeeking = (System.nanoTime() - mPanelPeekedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "EphemeralTab.DurationPeeked", durationPeeking);
        }
    }

    /** Starts timing the open state if it's not already been started. */
    private void startOpenTimer() {
        if (mPanelOpenedNanoseconds == 0) mPanelOpenedNanoseconds = System.nanoTime();
    }

    /** Finishes timing metrics for the first open state, unless that has already been done. */
    private void finishOpenTimer() {
        if (!mDidRecordFirstOpen && mPanelOpenedNanoseconds != 0) {
            mDidRecordFirstOpen = true;
            long durationOpened = (System.nanoTime() - mPanelOpenedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "EphemeralTab.DurationOpened", durationOpened);
        }
    }
}
