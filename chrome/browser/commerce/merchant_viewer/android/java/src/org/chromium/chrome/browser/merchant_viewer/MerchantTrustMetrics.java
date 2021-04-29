// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.IntDef;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.messages.DismissReason;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Metrics util class for merchant trust.
 */
public class MerchantTrustMetrics {
    /**
     * The reason why we clear the message in the queue of {@link MessageDispatcher}.
     *
     * Needs to stay in sync with MerchantTrustMessageClearReason in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({MessageClearReason.UNKNOWN, MessageClearReason.NAVIGATE_TO_SAME_DOMAIN,
            MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageClearReason {
        int UNKNOWN = 0;
        int NAVIGATE_TO_SAME_DOMAIN = 1;
        int NAVIGATE_TO_DIFFERENT_DOMAIN = 2;
        // Always update MAX_VALUE to match the last reason in the list.
        int MAX_VALUE = 2;
    }

    // Metrics for merchant trust signal message.
    private boolean mDidRecordMessagePrepared;
    private boolean mDidRecordMessageShown;
    private long mMessagePreparedNanoseconds;
    private long mMessageVisibleNanoseconds;

    // Metrics for merchant trust detailed bottom sheet.
    private boolean mDidRecordBottomSheetFirstPeek;
    private boolean mDidRecordBottomSheetFirstHalfOpen;
    private boolean mDidRecordBottomSheetFirstFullyOpen;
    private boolean mIsBottomSheetHalfViewed;
    private boolean mIsBottomSheetFullyViewed;
    private long mBottomSheetPeekedNanoseconds;
    private long mBottomSheetHalfOpenedNanoseconds;
    private long mBottomSheetFullyOpenedNanoseconds;

    /** Records metrics when the message is prepared. */
    public void recordMetricsForMessagePrepared() {
        startMessagePreparedTimer();
    }

    /** Records metrics when user leaves a rate-eligible page without seeing the message. */
    public void recordMetricsForMessageCleared(@MessageClearReason int clearReason) {
        finishMessagePreparedTimer();
        RecordHistogram.recordEnumeratedHistogram(
                "MerchantTrust.Message.ClearReason", clearReason, MessageClearReason.MAX_VALUE + 1);
        resetMessageMetrics();
    }

    /** Records metrics when the message is shown. */
    public void recordMetricsForMessageShown() {
        startMessageShownTimer();
        finishMessagePreparedTimer();
    }

    /** Records metrics when the message is dismissed. */
    public void recordMetricsForMessageDismissed(@DismissReason int dismissReason) {
        finishMessageShownTimer();
        RecordHistogram.recordEnumeratedHistogram(
                "MerchantTrust.Message.DismissReason", dismissReason, DismissReason.MAX_VALUE + 1);
        resetMessageMetrics();
    }

    /** Records metrics when the message is tapped. */
    public void recordMetricsForMessageTapped() {
        finishMessageShownTimer();
        RecordUserAction.record("MerchantTrust.Message.Tapped");
        resetMessageMetrics();
    }

    /** Starts timing when the message is prepared. */
    private void startMessagePreparedTimer() {
        mMessagePreparedNanoseconds = System.nanoTime();
    }

    /** Finishes timing when the message is cleared or shown. */
    private void finishMessagePreparedTimer() {
        if (!mDidRecordMessagePrepared && mMessagePreparedNanoseconds != 0) {
            mDidRecordMessagePrepared = true;
            long durationPrepared = (System.nanoTime() - mMessagePreparedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "MerchantTrust.Message.DurationPrepared", durationPrepared);
        }
    }

    /** Starts timing when the message is shown. */
    private void startMessageShownTimer() {
        mMessageVisibleNanoseconds = System.nanoTime();
    }

    /** Finishes timing when the message is dismissed or tapped. */
    private void finishMessageShownTimer() {
        if (!mDidRecordMessageShown && mMessageVisibleNanoseconds != 0) {
            mDidRecordMessageShown = true;
            long durationShow = (System.nanoTime() - mMessageVisibleNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "MerchantTrust.Message.DurationShown", durationShow);
        }
    }

    /** Resets all message-related metrics. */
    private void resetMessageMetrics() {
        mDidRecordMessagePrepared = false;
        mDidRecordMessageShown = false;
        mMessagePreparedNanoseconds = 0;
        mMessageVisibleNanoseconds = 0;
    }

    /** Records metrics for the peeked panel state. */
    public void recordMetricsForBottomSheetPeeked() {
        startBottomSheetPeekTimer();
        finishBottomSheetHalfOpenTimer();
        finishBottomSheetFullyOpenTimer();
    }

    /** Records metrics when the panel has been half opened. */
    public void recordMetricsForBottomSheetHalfOpened() {
        mIsBottomSheetHalfViewed = true;
        startBottomSheetHalfOpenTimer();
        finishBottomSheetPeekTimer();
        finishBottomSheetFullyOpenTimer();
    }

    /** Records metrics when the panel has been fully opened. */
    public void recordMetricsForBottomSheetFullyOpened() {
        mIsBottomSheetFullyViewed = true;
        startBottomSheetFullyOpenTimer();
        finishBottomSheetPeekTimer();
        finishBottomSheetHalfOpenTimer();
    }

    /** Records metrics when the panel has been closed. */
    public void recordMetricsForBottomSheetClosed(@StateChangeReason int stateChangeReason) {
        finishBottomSheetPeekTimer();
        finishBottomSheetHalfOpenTimer();
        finishBottomSheetFullyOpenTimer();
        RecordHistogram.recordBooleanHistogram(
                "MerchantTrust.BottomSheet.IsHalfViewed", mIsBottomSheetHalfViewed);
        RecordHistogram.recordBooleanHistogram(
                "MerchantTrust.BottomSheet.IsFullyViewed", mIsBottomSheetFullyViewed);
        RecordHistogram.recordEnumeratedHistogram("MerchantTrust.BottomSheet.CloseReason",
                stateChangeReason, StateChangeReason.MAX_VALUE + 1);
        resetBottomSheetMetrics();
    }

    /** Records a user action that navigates to a new link on the bottom sheet. */
    public void recordNavigateLinkOnBottomSheet() {
        RecordUserAction.record("MerchantTrust.BottomSheet.NavigateLink");
    }

    /** Resets all bottom sheet-related metrics. */
    private void resetBottomSheetMetrics() {
        mDidRecordBottomSheetFirstPeek = false;
        mDidRecordBottomSheetFirstHalfOpen = false;
        mDidRecordBottomSheetFirstFullyOpen = false;
        mIsBottomSheetHalfViewed = false;
        mIsBottomSheetFullyViewed = false;
        mBottomSheetPeekedNanoseconds = 0;
        mBottomSheetHalfOpenedNanoseconds = 0;
        mBottomSheetFullyOpenedNanoseconds = 0;
    }

    /** Starts timing the peek state if it's not already been started. */
    private void startBottomSheetPeekTimer() {
        if (mBottomSheetPeekedNanoseconds == 0) {
            mBottomSheetPeekedNanoseconds = System.nanoTime();
        }
    }

    /** Finishes timing metrics for the first peek state, unless that has already been done. */
    private void finishBottomSheetPeekTimer() {
        if (!mDidRecordBottomSheetFirstPeek && mBottomSheetPeekedNanoseconds != 0) {
            mDidRecordBottomSheetFirstPeek = true;
            long durationPeeking = (System.nanoTime() - mBottomSheetPeekedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "MerchantTrust.BottomSheet.DurationPeeked", durationPeeking);
        }
    }

    /** Starts timing the half open state if it's not already been started. */
    private void startBottomSheetHalfOpenTimer() {
        if (mBottomSheetHalfOpenedNanoseconds == 0) {
            mBottomSheetHalfOpenedNanoseconds = System.nanoTime();
        }
    }

    /** Finishes timing metrics for the first half open state, unless that has already been done. */
    private void finishBottomSheetHalfOpenTimer() {
        if (!mDidRecordBottomSheetFirstHalfOpen && mBottomSheetHalfOpenedNanoseconds != 0) {
            mDidRecordBottomSheetFirstHalfOpen = true;
            long durationOpened = (System.nanoTime() - mBottomSheetHalfOpenedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "MerchantTrust.BottomSheet.DurationHalfOpened", durationOpened);
        }
    }

    /** Starts timing the fully open state if it's not already been started. */
    private void startBottomSheetFullyOpenTimer() {
        if (mBottomSheetFullyOpenedNanoseconds == 0) {
            mBottomSheetFullyOpenedNanoseconds = System.nanoTime();
        }
    }

    /**
     * Finishes timing metrics for the first fully open state, unless that has already been done.
     */
    private void finishBottomSheetFullyOpenTimer() {
        if (!mDidRecordBottomSheetFirstFullyOpen && mBottomSheetFullyOpenedNanoseconds != 0) {
            mDidRecordBottomSheetFirstFullyOpen = true;
            long durationOpened = (System.nanoTime() - mBottomSheetFullyOpenedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "MerchantTrust.BottomSheet.DurationFullyOpened", durationOpened);
        }
    }
}