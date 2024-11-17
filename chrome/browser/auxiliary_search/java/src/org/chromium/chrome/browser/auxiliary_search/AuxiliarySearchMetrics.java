// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchBackgroundTask.DonateResult;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchDataType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class collects a variety of different Omnibox related metrics. */
public class AuxiliarySearchMetrics {
    // This enum is used to record UMA histograms, and should sync with AuxiliarySearchRequestStatus
    // in enums.xml.
    @IntDef({
        RequestStatus.SENT,
        RequestStatus.SUCCESSFUL,
        RequestStatus.UNSUCCESSFUL,
        RequestStatus.REJECTED,
        RequestStatus.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface RequestStatus {
        /** Indicates request was issued. */
        int SENT = 0;

        /** Indicates request was processed and succeeded. */
        int SUCCESSFUL = 1;

        /** Indicates request was processed but failed. */
        int UNSUCCESSFUL = 2;

        /** Indicates request was rejected without processing (e.g. ineligible device). */
        int REJECTED = 3;

        int NUM_ENTRIES = 4;
    }

    /** Captures the amount of time spent deleting all content. */
    @VisibleForTesting
    public static final String HISTOGRAM_DELETION_TIME = "Search.AuxiliarySearch.DeleteTime";

    /** Captures the amount of time spent deleting bookmark content. */
    @VisibleForTesting
    public static final String HISTOGRAM_DELETION_BOOKMARK_TIME =
            "Search.AuxiliarySearch.DeleteTime.Bookmarks";

    /** Captures the amount of time spent deleting tab content. */
    @VisibleForTesting
    public static final String HISTOGRAM_DELETION_TAB_TIME =
            "Search.AuxiliarySearch.DeleteTime.Tabs";

    /** Captures the amount of time spent donating content. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATION_TIME = "Search.AuxiliarySearch.DonateTime";

    /** Captures the status of the AuxiliarySearch all content deletion requests. */
    @VisibleForTesting
    public static final String HISTOGRAM_DELETION_STATUS =
            "Search.AuxiliarySearch.DeletionRequestStatus";

    /** Captures the status of the AuxiliarySearch bookmark content deletion requests. */
    @VisibleForTesting
    public static final String HISTOGRAM_DELETION_BOOKMARK_STATUS =
            "Search.AuxiliarySearch.DeletionRequestStatus.Bookmarks";

    /** Captures the status of the AuxiliarySearch tab content deletion requests. */
    @VisibleForTesting
    public static final String HISTOGRAM_DELETION_TAB_STATUS =
            "Search.AuxiliarySearch.DeletionRequestStatus.Tabs";

    /** Captures the status of the AuxiliarySearch content donation requests. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATION_STATUS =
            "Search.AuxiliarySearch.DonationRequestStatus";

    /** Captures the amount of time spent querying bookmarks. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_BOOKMARKS =
            "Search.AuxiliarySearch.QueryTime.Bookmarks";

    /** Captures the amount of time spent querying tabs. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_TABS = "Search.AuxiliarySearch.QueryTime.Tabs";

    /** Captures the amount of time spent querying the favicons of tabs. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_FAVICONS =
            "Search.AuxiliarySearch.QueryTime.Favicons";

    /** Captures the cumulative amount of time spent querying searchable data. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_TOTAL = "Search.AuxiliarySearch.QueryTime";

    /** Captures the amount of donated bookmarks. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATEDCOUNT_BOOKMARKS =
            "Search.AuxiliarySearch.DonationSent.Bookmarks";

    /** Captures the amount of donated tabs. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATEDCOUNT_TABS =
            "Search.AuxiliarySearch.DonationSent.Tabs";

    /** Captures the amount of all donated content. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATEDCOUNT_TOTAL = "Search.AuxiliarySearch.DonationSent";

    /** Captures the total number of favicons available for the first donation. */
    @VisibleForTesting
    public static final String HISTOGRAM_FAVICON_FIRST_DONATE_COUNT =
            "Search.AuxiliarySearch.Favicon.FirstDonateCount";

    /** Captures the amount of time spent donating content for a scheduled background task. */
    @VisibleForTesting
    public static final String HISTOGRAM_SCHEDULE_DONATION_TIME =
            "Search.AuxiliarySearch.Schedule.DonateTime";

    private static final String SCHEDULE_DELAY_TIME_UMA =
            "Search.AuxiliarySearch.Schedule.DelayTime";
    private static final String SCHEDULE_DONATE_RESULT_UMA =
            "Search.AuxiliarySearch.Schedule.FaviconDonateResult";
    private static final String SCHEDULE_FAVICON_DONATE_COUNT_UMA =
            "Search.AuxiliarySearch.Schedule.Favicon.DonateCount";
    private static final String SCHEDULE_FAVICON_FETCH_TIME_UMA =
            "Search.AuxiliarySearch.Schedule.Favicon.FetchTime";

    /** Record the amount of time spent deleting content from the auxiliary search. */
    public static void recordDeleteTime(
            long deleteTimeInMs, @AuxiliarySearchDataType int datatype) {
        String histogram = null;
        switch (datatype) {
            case AuxiliarySearchDataType.ALL:
                histogram = HISTOGRAM_DELETION_TIME;
                break;
            case AuxiliarySearchDataType.BOOKMARK:
                histogram = HISTOGRAM_DELETION_BOOKMARK_TIME;
                break;
            case AuxiliarySearchDataType.TAB:
                histogram = HISTOGRAM_DELETION_TAB_TIME;
                break;
            default:
                // unsupported datatype.
                break;
        }
        if (histogram == null) return;
        RecordHistogram.recordTimesHistogram(histogram, deleteTimeInMs);
    }

    /** Record the amount of time spent donating content to the auxiliary search. */
    public static void recordDonateTime(long donateTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_DONATION_TIME, donateTimeInMs);
    }

    /**
     * Record the amount of time spent donating content to the auxiliary search for a scheduled
     * background task.
     */
    public static void recordScheduledDonateTime(long donateTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_SCHEDULE_DONATION_TIME, donateTimeInMs);
    }

    /** Record the status of the request of the Auxiliary Search content deletion. */
    public static void recordDeletionRequestStatus(
            @RequestStatus int status, @AuxiliarySearchDataType int datatype) {
        String histogram = null;
        switch (datatype) {
            case AuxiliarySearchDataType.ALL:
                histogram = HISTOGRAM_DELETION_STATUS;
                break;
            case AuxiliarySearchDataType.BOOKMARK:
                histogram = HISTOGRAM_DELETION_BOOKMARK_STATUS;
                break;
            case AuxiliarySearchDataType.TAB:
                histogram = HISTOGRAM_DELETION_TAB_STATUS;
                break;
            default:
                // unsupported datatype.
                break;
        }
        if (histogram == null) return;
        RecordHistogram.recordEnumeratedHistogram(histogram, status, RequestStatus.NUM_ENTRIES);
    }

    /** Record the status of the request of the Auxiliary Search content donation. */
    public static void recordDonationRequestStatus(@RequestStatus int status) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_DONATION_STATUS, status, RequestStatus.NUM_ENTRIES);
    }

    /** Record the amount of time for querying bookmarks. */
    public static TimingMetric recordQueryBookmarksTime() {
        return TimingMetric.mediumUptime(HISTOGRAM_QUERYTIME_BOOKMARKS);
    }

    /** Record the amount of time for querying tabs. */
    public static void recordQueryTabTime(long queryTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_QUERYTIME_TABS, queryTimeInMs);
    }

    /** Record the amount of time for querying bookmarks and tabs. */
    public static void recordTotalQueryTime(long queryTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_QUERYTIME_TOTAL, queryTimeInMs);
    }

    /** Record the count of bookmarks and tabs donated to Auxiliary Search. */
    public static void recordDonationCount(int bookmarkCount, int tabCount) {
        RecordHistogram.recordCount100Histogram(HISTOGRAM_DONATEDCOUNT_BOOKMARKS, bookmarkCount);
        RecordHistogram.recordCount100Histogram(HISTOGRAM_DONATEDCOUNT_TABS, tabCount);
        RecordHistogram.recordCount1000Histogram(
                HISTOGRAM_DONATEDCOUNT_TOTAL, bookmarkCount + tabCount);
    }

    /** Record the amount of time for querying a favicon. */
    public static void recordQueryFaviconTime(long queryTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_QUERYTIME_FAVICONS, queryTimeInMs);
    }

    /**
     * Records the count of available favicons for the first donation.
     *
     * @param count The number of favicons available once all fetching is completed.
     */
    public static void recordFaviconFirstDonationCount(int count) {
        RecordHistogram.recordCount100Histogram(HISTOGRAM_FAVICON_FIRST_DONATE_COUNT, count);
    }

    /** Records the delay time between a background task is created and is scheduled. */
    static void recordScheduledDelayTime(long delayTimeMs) {
        RecordHistogram.recordLongTimesHistogram(SCHEDULE_DELAY_TIME_UMA, delayTimeMs);
    }

    /** Records the duration of all favicon fetching via a background task. */
    static void recordScheduledFaviconFetchDuration(long fetchDurationMs) {
        RecordHistogram.recordMediumTimesHistogram(
                SCHEDULE_FAVICON_FETCH_TIME_UMA, fetchDurationMs);
    }

    /** Records the result of a scheduled donation via a background task. */
    static void recordScheduledDonationResult(@DonateResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                SCHEDULE_DONATE_RESULT_UMA, result, DonateResult.MAX_COUNT);
    }

    /** Records the total number of favicon donated via a background task. */
    static void recordScheduledFaviconDonateCount(int size) {
        RecordHistogram.recordCount1000Histogram(SCHEDULE_FAVICON_DONATE_COUNT_UMA, size);
    }
}
