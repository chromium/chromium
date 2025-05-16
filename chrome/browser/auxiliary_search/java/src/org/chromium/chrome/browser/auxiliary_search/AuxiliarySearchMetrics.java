// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import android.content.Intent;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchBackgroundTask.DonateResult;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController.AuxiliarySearchDataType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class collects a variety of different Omnibox related metrics. */
@NullMarked
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

    @IntDef({
        ClickInfo.OPT_IN,
        ClickInfo.OPT_OUT,
        ClickInfo.OPEN_SETTINGS,
        ClickInfo.TURN_ON,
        ClickInfo.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClickInfo {
        // Indicates users agree to keep the enabled state: clicking the "Got it" button.
        int OPT_IN = 0;
        // Indicates users agree to keep the disabled state: clicking the "No thanks" button.
        int OPT_OUT = 1;
        // Indicates users want to go to settings to config the status: clicking the
        // "Go to settings" button
        int OPEN_SETTINGS = 2;
        // Indicates users want to enable the feature which is disabled by default: clicking the
        // "Turn on" button.
        int TURN_ON = 3;
        int NUM_ENTRIES = 4;
    }

    public static final String CLICKED_ENTRY_TYPE = "com.android.chrome.clicked_data_type";
    public static final String CLICKED_ENTRY_POSITION = "com.android.chrome.clicked_data_position";

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

    /** Captures the amount of time spent querying tabs. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_TABS = "Search.AuxiliarySearch.QueryTime.Tabs";

    /** Captures the amount of time spent querying history database, e.g, Tabs and CCTs. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_HISTORY =
            "Search.AuxiliarySearch.QueryTime.History";

    /** Captures the amount of time spent querying the favicons of tabs. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_FAVICONS =
            "Search.AuxiliarySearch.QueryTime.Favicons";

    /** Captures the amount of time spent querying history database for CCTs. */
    @VisibleForTesting
    public static final String HISTOGRAM_QUERYTIME_CUSTOM_TABS =
            "Search.AuxiliarySearch.QueryTime.CustomTabs";

    /** Captures the amount of donated tabs. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATEDCOUNT_TABS =
            "Search.AuxiliarySearch.DonationCount.Tabs";

    /** Captures the amount of donated CCTs. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATEDCOUNT_CCTS =
            "Search.AuxiliarySearch.DonationCount.CustomTabs";

    /** Captures the amount of donated MVTs. */
    @VisibleForTesting
    public static final String HISTOGRAM_DONATEDCOUNT_TOP_SITES =
            "Search.AuxiliarySearch.DonationCount.TopSites";

    /** Captures the total number of favicons available for the first donation. */
    @VisibleForTesting
    public static final String HISTOGRAM_FAVICON_FIRST_DONATE_COUNT =
            "Search.AuxiliarySearch.Favicon.FirstDonateCount";

    /** Captures the amount of time spent donating content for a scheduled background task. */
    @VisibleForTesting
    public static final String HISTOGRAM_SCHEDULE_DONATION_TIME =
            "Search.AuxiliarySearch.Schedule.DonateTime";

    /** The maximum position logged in metrics. */
    @VisibleForTesting static final int MAX_POSITION_INDEX = 9;

    // 20 minutes in milliseconds.
    private static final long MAX_CONTROLLER_CREATION_DELAY_MS = DateUtils.MINUTE_IN_MILLIS * 20L;
    private static final String ENTRY_TYPE_TABS = ".Tabs";
    private static final String ENTRY_TYPE_CUSTOM_TABS = ".CustomTabs";
    private static final String ENTRY_TYPE_TOP_SITES = ".TopSites";
    private static final String SCHEDULE_DELAY_TIME_UMA =
            "Search.AuxiliarySearch.Schedule.DelayTime";
    private static final String SCHEDULE_DONATE_RESULT_UMA =
            "Search.AuxiliarySearch.Schedule.FaviconDonateResult";
    private static final String SCHEDULE_FAVICON_DONATE_COUNT_UMA =
            "Search.AuxiliarySearch.Schedule.Favicon.DonateCount";
    private static final String SCHEDULE_FAVICON_FETCH_TIME_UMA =
            "Search.AuxiliarySearch.Schedule.Favicon.FetchTime";
    private static final String HISTOGRAM_SHARE_TABS_WITH_OS =
            "Search.AuxiliarySearch.ShareTabsWithOs";
    private static final String HISTOGRAM_MODULE_CONSENT =
            "Search.AuxiliarySearch.Module.ClickInfo";

    private static final String HISTOGRAM_LAUNCHED_FROM_EXTERNAL_APP_PREFIX =
            "Search.AuxiliarySearch.LaunchedFromExternalApp.";
    private static final String HISTOGRAM_TOP_SITE_EXPIRATION_DURATION =
            "Search.AuxiliarySearch.TopSites.ExpirationDuration";

    private static final String HISTOGRAM_TIME_TO_CREATE_CONTROLLER_IN_CUSTOM_TAB =
            "Search.AuxiliarySearch.TimeToCreateControllerInCustomTab";

    @VisibleForTesting
    public static final String HISTOGRAM_CUSTOM_TAB_FETCH_RESULTS_COUNT =
            "Search.AuxiliarySearch.CustomTabFetchResults.Count";

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

    /** Record the amount of time for querying tabs. */
    public static void recordQueryTabTime(long queryTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_QUERYTIME_TABS, queryTimeInMs);
    }

    /** Record the amount of time for querying tabs and CCTs from the history database. */
    public static void recordQueryHistoryDataTime(long queryTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_QUERYTIME_HISTORY, queryTimeInMs);
    }

    /** Record the amount of time for querying CCTs from the history database. */
    public static void recordQueryCustomTabTime(long queryTimeInMs) {
        RecordHistogram.recordTimesHistogram(HISTOGRAM_QUERYTIME_CUSTOM_TABS, queryTimeInMs);
    }

    /**
     * Record the count of different entry types donated to Auxiliary Search, e.g, Tabs, CCTs and
     * top sites. If the count of any type is 0, we don't log the type.
     */
    public static void recordDonationCount(int[] counts) {
        for (int i = 0; i < counts.length; i++) {
            if (counts[i] == 0) continue;

            switch (i) {
                case AuxiliarySearchEntryType.TAB -> RecordHistogram.recordCount100Histogram(
                        HISTOGRAM_DONATEDCOUNT_TABS, counts[i]);
                case AuxiliarySearchEntryType.CUSTOM_TAB -> RecordHistogram.recordCount100Histogram(
                        HISTOGRAM_DONATEDCOUNT_CCTS, counts[i]);
                case AuxiliarySearchEntryType.TOP_SITE -> RecordHistogram.recordCount100Histogram(
                        HISTOGRAM_DONATEDCOUNT_TOP_SITES, counts[i]);
            }
        }
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

    /** Records whether sharing Tabs with the system is enabled. */
    static void recordIsShareTabsWithOsEnabled(boolean enabled) {
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_SHARE_TABS_WITH_OS, enabled);
    }

    /** Records the type of the button clicked on the module. */
    public static void recordClickButtonInfo(@ClickInfo int type) {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_MODULE_CONSENT, type, ClickInfo.NUM_ENTRIES);
    }

    @VisibleForTesting
    @Nullable
    static String getEntryTypeString(@AuxiliarySearchEntryType int type) {
        switch (type) {
            case AuxiliarySearchEntryType.TAB:
                return ENTRY_TYPE_TABS;
            case AuxiliarySearchEntryType.CUSTOM_TAB:
                return ENTRY_TYPE_CUSTOM_TABS;
            case AuxiliarySearchEntryType.TOP_SITE:
                return ENTRY_TYPE_TOP_SITES;
            default:
                return null;
        }
    }

    /**
     * Records the date type and the position when a donated data is clicked from an supported
     * external app.
     *
     * @param externalAppName The name of the external app.
     * @param intent The launch intent.
     * @return Whether the metric is logged successfully.
     */
    public static boolean maybeRecordExternalAppClickInfo(String externalAppName, Intent intent) {
        int type = IntentUtils.safeGetIntExtra(intent, CLICKED_ENTRY_TYPE, -1);
        if (type < AuxiliarySearchEntryType.TAB || type > AuxiliarySearchEntryType.MAX_VALUE) {
            return false;
        }

        int position = IntentUtils.safeGetIntExtra(intent, CLICKED_ENTRY_POSITION, -1);
        if (position < 0) {
            return false;
        }

        StringBuilder builder = new StringBuilder();
        builder.append(HISTOGRAM_LAUNCHED_FROM_EXTERNAL_APP_PREFIX);
        builder.append(externalAppName);
        builder.append(getEntryTypeString(type));
        if (position > MAX_POSITION_INDEX) {
            position = MAX_POSITION_INDEX;
        }

        RecordHistogram.recordEnumeratedHistogram(builder.toString(), position, MAX_POSITION_INDEX);
        return true;
    }

    /**
     * Records the expiration duration of the latest fetch results of the top sites.
     *
     * @param expirationDurationMs How long ago the fetch results expired in milliseconds.
     */
    static void recordTopSiteExpirationDuration(long expirationDurationMs) {
        RecordHistogram.recordCustomTimesHistogram(
                HISTOGRAM_TOP_SITE_EXPIRATION_DURATION,
                expirationDurationMs,
                /* min= */ 1,
                DateUtils.DAY_IN_MILLIS,
                /* numBuckets= */ 50);
    }

    /**
     * Records the time from CustomTabActivity#onCreate() until AuxiliarySearchController is created
     * in CustomTabActivity#onDeferredStartup().
     *
     * @param timeToCreateControllerMs The time in milliseconds from when the activity is created
     *     until the AuxiliarySearchController is created.
     */
    public static void recordTimeToCreateControllerInCustomTab(long timeToCreateControllerMs) {
        RecordHistogram.recordCustomTimesHistogram(
                HISTOGRAM_TIME_TO_CREATE_CONTROLLER_IN_CUSTOM_TAB,
                timeToCreateControllerMs,
                /* min= */ 1,
                MAX_CONTROLLER_CREATION_DELAY_MS,
                /* numBuckets= */ 50);
    }

    /**
     * Records the count of entries obtained from fetching history database initiated by a Custom
     * Tab.
     *
     * @param count The total number of entries returned from fetching history database.
     */
    public static void recordCustomTabFetchResultsCount(int count) {
        RecordHistogram.recordCount100Histogram(HISTOGRAM_CUSTOM_TAB_FETCH_RESULTS_COUNT, count);
    }
}
