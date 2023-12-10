// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Metrics util class for merchant trust. */
public class MerchantTrustMetrics {
    @VisibleForTesting
    public static String MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM =
            "MerchantTrust.MessageImpact.BrowsingTime";

    @VisibleForTesting
    public static String MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM =
            "MerchantTrust.MessageImpact.NavigationCount";

    /**
     * The reason why we clear the prepared message.
     *
     * Needs to stay in sync with MerchantTrustMessageClearReason in enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        MessageClearReason.UNKNOWN,
        MessageClearReason.NAVIGATE_TO_SAME_DOMAIN,
        MessageClearReason.NAVIGATE_TO_DIFFERENT_DOMAIN,
        MessageClearReason.MESSAGE_CONTEXT_NO_LONGER_VALID,
        MessageClearReason.SWITCH_TO_DIFFERENT_WEBCONTENTS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageClearReason {
        int UNKNOWN = 0;
        int NAVIGATE_TO_SAME_DOMAIN = 1;
        int NAVIGATE_TO_DIFFERENT_DOMAIN = 2;
        int MESSAGE_CONTEXT_NO_LONGER_VALID = 3;
        int SWITCH_TO_DIFFERENT_WEBCONTENTS = 4;
        // Always update MAX_VALUE to match the last reason in the list.
        int MAX_VALUE = 4;
    }

    /**
     * Which ui the bottom sheet is opened from.
     *
     * Needs to stay in sync with MerchantTrustBottomSheetOpenedSource in enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        BottomSheetOpenedSource.UNKNOWN,
        BottomSheetOpenedSource.FROM_MESSAGE,
        BottomSheetOpenedSource.FROM_PAGE_INFO
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BottomSheetOpenedSource {
        int UNKNOWN = 0;
        int FROM_MESSAGE = 1;
        int FROM_PAGE_INFO = 2;
        // Always update MAX_VALUE to match the last item in the list.
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

    // Metrics for message impact on user browsing.
    private long mMessageVisibleNsForBrowsingTime;
    private int mNavigationCountAfterMessageShown;
    private double mMessageStarRating;
    private String mCurrentHost;

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
                "MerchantTrust.Message.DismissReason", dismissReason, DismissReason.COUNT);
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
            long durationPrepared =
                    (System.nanoTime() - mMessagePreparedNanoseconds)
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
            long durationShow =
                    (System.nanoTime() - mMessageVisibleNanoseconds)
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
        RecordHistogram.recordEnumeratedHistogram(
                "MerchantTrust.BottomSheet.CloseReason",
                stateChangeReason,
                StateChangeReason.MAX_VALUE + 1);
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
            long durationPeeking =
                    (System.nanoTime() - mBottomSheetPeekedNanoseconds)
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
            long durationOpened =
                    (System.nanoTime() - mBottomSheetHalfOpenedNanoseconds)
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
            long durationOpened =
                    (System.nanoTime() - mBottomSheetFullyOpenedNanoseconds)
                            / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "MerchantTrust.BottomSheet.DurationFullyOpened", durationOpened);
        }
    }

    /** Records metrics when the page info is opened. */
    public void recordMetricsForStoreInfoRowVisible(boolean visible) {
        RecordHistogram.recordBooleanHistogram(
                "MerchantTrust.PageInfo.IsStoreInfoVisible", visible);
    }

    /** Records metrics for the bottom sheet opened source. */
    public void recordMetricsForBottomSheetOpenedSource(@BottomSheetOpenedSource int source) {
        RecordHistogram.recordEnumeratedHistogram(
                "MerchantTrust.BottomSheet.OpenSource",
                source,
                BottomSheetOpenedSource.MAX_VALUE + 1);
    }

    /** Start recording message impact on user browsing time and navigation times. */
    public void startRecordingMessageImpact(String hostName, double starRating) {
        mMessageVisibleNsForBrowsingTime = System.nanoTime();
        mNavigationCountAfterMessageShown = 0;
        mCurrentHost = hostName;
        mMessageStarRating = starRating;
    }

    /** Update the message impact data. */
    public void updateRecordingMessageImpact(String hostName) {
        if (mCurrentHost != null) {
            if (mCurrentHost.equals(hostName)) {
                mNavigationCountAfterMessageShown++;
            } else {
                finishRecordingMessageImpact();
            }
        }
    }

    /** Finish recording message impact for this host and reset the data. */
    public void finishRecordingMessageImpact() {
        if (mCurrentHost != null) {
            long browsingTime =
                    (System.nanoTime() - mMessageVisibleNsForBrowsingTime)
                            / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordCustomTimesHistogram(
                    MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM,
                    browsingTime,
                    10,
                    DateUtils.MINUTE_IN_MILLIS * 10,
                    50);
            RecordHistogram.recordCustomTimesHistogram(
                    MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM + getStarRatingSuffixForMessageImpact(),
                    browsingTime,
                    10,
                    DateUtils.MINUTE_IN_MILLIS * 10,
                    50);

            RecordHistogram.recordCount100Histogram(
                    MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM, mNavigationCountAfterMessageShown);
            RecordHistogram.recordCount100Histogram(
                    MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM
                            + getStarRatingSuffixForMessageImpact(),
                    mNavigationCountAfterMessageShown);
        }
        mMessageVisibleNsForBrowsingTime = 0;
        mNavigationCountAfterMessageShown = 0;
        mCurrentHost = null;
        mMessageStarRating = 0.0;
    }

    /**
     * To better analyze the message impact, we add a suffix to each histogram based on the shown
     * star rating.
     */
    private String getStarRatingSuffixForMessageImpact() {
        // Only keep one decimal to avoid inaccurate double value such as 4.49999.
        double ratingValue = Math.round(mMessageStarRating * 10) / 10.0;
        String ratingSuffix;
        if (ratingValue >= 4.5) {
            ratingSuffix = "AboveFourPointFive";
        } else if (ratingValue >= 4.0) {
            ratingSuffix = "AboveFour";
        } else if (ratingValue >= 3.0) {
            ratingSuffix = "AboveThree";
        } else if (ratingValue >= 2.0) {
            ratingSuffix = "AboveTwo";
        } else {
            ratingSuffix = "BelowTwo";
        }
        return ".Rating" + ratingSuffix;
    }

    /** Record ukm when merchant trust data is available. */
    public void recordUkmOnDataAvailable(@Nullable WebContents webContents) {
        recordBooleanUkm(webContents, "Shopping.MerchantTrust.DataAvailable", "DataAvailable");
    }

    /** Record ukm when merchant trust message is displayed. */
    public void recordUkmOnMessageSeen(@Nullable WebContents webContents) {
        recordBooleanUkm(webContents, "Shopping.MerchantTrust.MessageSeen", "HasOccurred");
    }

    /** Record ukm when merchant trust message is clicked. */
    public void recordUkmOnMessageClicked(@Nullable WebContents webContents) {
        recordBooleanUkm(webContents, "Shopping.MerchantTrust.MessageClicked", "HasOccurred");
    }

    /** Record ukm when store info row in trusted surface is displayed. */
    public void recordUkmOnRowSeen(@Nullable WebContents webContents) {
        recordBooleanUkm(webContents, "Shopping.MerchantTrust.RowSeen", "HasOccurred");
    }

    /** Record ukm when store info row in trusted surface is clicked. */
    public void recordUkmOnRowClicked(@Nullable WebContents webContents) {
        recordBooleanUkm(webContents, "Shopping.MerchantTrust.RowClicked", "HasOccurred");
    }

    private void recordBooleanUkm(
            @Nullable WebContents webContents, String eventName, String metricsName) {
        if (webContents != null) {
            new UkmRecorder.Bridge()
                    .recordEventWithBooleanMetric(webContents, eventName, metricsName);
        }
    }
}
