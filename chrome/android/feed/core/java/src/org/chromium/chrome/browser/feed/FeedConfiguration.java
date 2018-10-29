// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.text.TextUtils;

import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Collection of configurable parameters and default values given to the Feed. Every getter passes
 * checks to see if it has been overridden by a field trail param.
 */
public final class FeedConfiguration {
    /** Do not allow construction */
    private FeedConfiguration() {}

    private static final String FEED_SERVER_ENDPOINT = "feed_server_endpoint";
    /** Default value for server endpoint. */
    public static final String FEED_SERVER_ENDPOINT_DEFAULT =
            "https://www.google.com/httpservice/noretry/NowStreamService/FeedQuery";

    private static final String FEED_SERVER_METHOD = "feed_server_method";
    /** Default value for feed server method. */
    public static final String FEED_SERVER_METHOD_DEFAULT = "GET";

    private static final String FEED_SERVER_RESPONSE_LENGTH_PREFIXED =
            "feed_server_response_length_prefixed";
    /** Default value for feed server response length prefixed. */
    public static final boolean FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT = true;

    private static final String LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS =
            "logging_immediate_content_threshold_ms";
    /** Default value for logging immediate content threshold. */
    public static final int LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT = 1000;

    private static final String SESSION_LIFETIME_MS = "session_lifetime_ms";
    /** Default value for session lifetime. */
    public static final int SESSION_LIFETIME_MS_DEFAULT = 3600000;

    private static final String TRIGGER_IMMEDIATE_PAGINATION = "trigger_immediate_pagination";
    /** Default value for triggering immediate pagination. */
    public static final boolean TRIGGER_IMMEDIATE_PAGINATION_DEFAULT = false;

    private static final String VIEW_LOG_THRESHOLD = "view_log_threshold";
    /** Default value for logging view threshold. */
    public static final double VIEW_LOG_THRESHOLD_DEFAULT = 0.66d;

    /** @return Feed server endpoint to use to fetch content suggestions. */
    @VisibleForTesting
    static String getFeedServerEndpoint() {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_SERVER_ENDPOINT);
        return TextUtils.isEmpty(paramValue) ? FEED_SERVER_ENDPOINT_DEFAULT : paramValue;
    }

    /** @return Feed server method to use when fetching content suggestions. */
    @VisibleForTesting
    static String getFeedServerMethod() {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_SERVER_METHOD);
        return TextUtils.isEmpty(paramValue) ? FEED_SERVER_METHOD_DEFAULT : paramValue;
    }

    /** @return Whether server response should be length prefixed. */
    @VisibleForTesting
    static boolean getFeedServerReponseLengthPrefixed() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                FEED_SERVER_RESPONSE_LENGTH_PREFIXED, FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT);
    }

    /**
     * @return How long before showing content after opening NTP is no longer considered immediate
     *         in UMA.
     */
    @VisibleForTesting
    static long getLoggingImmediateContentThresholdMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT);
    }

    /** @return Time until feed stops restoring the UI. */
    @VisibleForTesting
    static long getSessionLifetimeMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SESSION_LIFETIME_MS,
                SESSION_LIFETIME_MS_DEFAULT);
    }

    /**
     * @return Whether UI initially shows "More" button upon reaching the end of known content,
     *         when server could potentially have more content.
     */
    @VisibleForTesting
    static boolean getTriggerImmedatePagination() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, TRIGGER_IMMEDIATE_PAGINATION,
                TRIGGER_IMMEDIATE_PAGINATION_DEFAULT);
    }

    /** @return How much of a card must be on screen to generate a UMA log view. */
    @VisibleForTesting
    static double getViewLogThreshold() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, VIEW_LOG_THRESHOLD,
                VIEW_LOG_THRESHOLD_DEFAULT);
    }

    /**
     * @return A fully built {@link Configuration}, ready to be given to the Feed.
     */
    public static Configuration createConfiguration() {
        return new Configuration.Builder()
                .put(ConfigKey.FEED_SERVER_ENDPOINT, FeedConfiguration.getFeedServerEndpoint())
                .put(ConfigKey.FEED_SERVER_METHOD, FeedConfiguration.getFeedServerMethod())
                .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED,
                        FeedConfiguration.getFeedServerReponseLengthPrefixed())
                .put(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                        FeedConfiguration.getLoggingImmediateContentThresholdMs())
                .put(ConfigKey.SESSION_LIFETIME_MS, FeedConfiguration.getSessionLifetimeMs())
                .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION,
                        FeedConfiguration.getTriggerImmedatePagination())
                .put(ConfigKey.VIEW_LOG_THRESHOLD, FeedConfiguration.getViewLogThreshold())
                .build();
    }
}
