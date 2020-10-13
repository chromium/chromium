// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Collection of configurable parameters and default values given to the Feed. Every getter passes
 * checks to see if it has been overridden by a field trail param.
 */
public final class FeedConfiguration {
    /** Do not allow construction */
    private FeedConfiguration() {}

    private static final String ABANDON_RESTORE_BELOW_FOLD = "abandon_restore_below_fold";
    /** Default value for whether to restore below fold. */
    public static final boolean ABANDON_RESTORE_BELOW_FOLD_DEFAULT = false;

    private static final String CARD_MENU_TOOLTIP_ELIGIBLE = "card_menu_tooltip_eligible";
    /** Default value for if card menus should have tooltips enabled. */
    public static final boolean CARD_MENU_TOOLTIP_ELIGIBLE_DEFAULT = true;

    private static final String CONSUME_SYNTHETIC_TOKENS = "consume_synthetic_tokens_bool";
    /** Default value for whether to consumer synthetic tokens on load. */
    public static final boolean CONSUME_SYNTHETIC_TOKENS_DEFAULT = false;

    private static final String CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING =
            "consume_synthetic_tokens_while_restoring_bool";
    /** Default value for whether to consumer synthetic tokens on restore. */
    public static final boolean CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING_DEFAULT = true;

    private static final String DEFAULT_ACTION_TTL_SECONDS = "default_action_ttl_seconds";
    /** Default value for the TTL of default action (25hrs). */
    public static final long DEFAULT_ACTION_TTL_SECONDS_DEFAULT = 90000;

    private static final String FEED_ACTION_SERVER_ENDPOINT = "feed_action_server_endpoint";
    /** Default value for the endpoint used for recording uploaded actions to the server. */
    public static final String FEED_ACTION_SERVER_ENDPOINT_DEFAULT =
            "https://www.google.com/httpservice/retry/ClankActionUploadService/ClankActionUpload";

    private static final String FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST =
            "feed_action_server_max_actions_per_request";
    /**
     * Default value for maximum number of actions to be uploaded to the enpoint in a single
     * request.
     */
    public static final long FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST_DEFAULT = 20;

    private static final String FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST =
            "feed_action_server_max_size_per_request";
    /**
     * Default value for maximum size in bytes of the request to be uploaded to the enpoint in a
     * single request.
     */
    public static final long FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST_DEFAULT = 1000;

    private static final String FEED_ACTION_SERVER_METHOD = "feed_action_server_method";
    /** Default value for the HTTP method call to the feed action server (put/post/etc). */
    public static final String FEED_ACTION_SERVER_METHOD_DEFAULT = "POST";

    private static final String FEED_SERVER_ENDPOINT = "feed_server_endpoint";
    /** Default value for server endpoint. */
    public static final String FEED_SERVER_ENDPOINT_DEFAULT =
            "https://www.google.com/httpservice/noretry/DiscoverClankService/FeedQuery";

    private static final String FEED_SERVER_METHOD = "feed_server_method";
    /** Default value for feed server method. */
    public static final String FEED_SERVER_METHOD_DEFAULT = "GET";

    private static final String FEED_SERVER_RESPONSE_LENGTH_PREFIXED =
            "feed_server_response_length_prefixed";
    /** Default value for feed server response length prefixed. */
    public static final boolean FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT = true;

    private static final String FEED_UI_ENABLED = "feed_ui_enabled";
    /** Default value for the type of UI to request from the server. */
    public static final boolean FEED_UI_ENABLED_DEFAULT = true;

    private static final String INITIAL_NON_CACHED_PAGE_SIZE = "initial_non_cached_page_size";
    /** Default value for initial non cached page size. */
    public static final long INITIAL_NON_CACHED_PAGE_SIZE_DEFAULT = 10;

    private static final String LIMIT_PAGE_UPDATES_IN_HEAD = "limit_page_updates_in_head";
    /** Default value for whether to update HEAD when making a page request. */
    public static final boolean LIMIT_PAGE_UPDATES_IN_HEAD_DEFAULT = false;

    private static final String LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS =
            "logging_immediate_content_threshold_ms";
    /** Default value for logging immediate content threshold. */
    public static final long LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT = 1000;

    private static final String MANAGE_INTERESTS_ENABLED = "manage_interests_enabled";
    /** Default value for whether to use menu options to launch interest management page. */
    public static final boolean MANAGE_INTERESTS_ENABLED_DEFAULT = true;

    private static final String SEND_FEEDBACK_ENABLED = "send_feedback_enabled";

    private static final String MAXIMUM_GC_ATTEMPTS = "maximum_gc_attempts";
    /** Default value for the maximum number of times that the GC task can re-enqueue itself. */
    public static final long MAXIMUM_GC_ATTEMPTS_DEFAULT = 10;

    private static final String NON_CACHED_MIN_PAGE_SIZE = "non_cached_min_page_size";
    /** Default value for non cached minimum page size. */
    public static final long NON_CACHED_MIN_PAGE_SIZE_DEFAULT = 5;

    private static final String NON_CACHED_PAGE_SIZE = "non_cached_page_size";
    /** Default value for non cached page size. */
    public static final long NON_CACHED_PAGE_SIZE_DEFAULT = 25;

    private static final String SESSION_LIFETIME_MS = "session_lifetime_ms";
    /** Default value for session lifetime. */
    public static final long SESSION_LIFETIME_MS_DEFAULT = 3600000;

    private static final String SNIPPETS_ENABLED = "snippets_enabled";
    /** Default value for whether to show article snippets. */
    public static final boolean SNIPPETS_ENABLED_DEFAULT = true;

    private static final String SPINNER_DELAY_MS = "spinner_delay";
    /** Default value for delay before showing a spinner. */
    public static final long SPINNER_DELAY_MS_DEFAULT = 500;

    private static final String SPINNER_MINIMUM_SHOW_TIME_MS = "spinner_minimum_show_time";
    /** Default value for how long spinners must be shown for. */
    public static final long SPINNER_MINIMUM_SHOW_TIME_MS_DEFAULT = 0;

    private static final String STORAGE_MISS_THRESHOLD = "storage_miss_threshold";
    /** Default number of items that can be missing from a call to FeedStore before failing. */
    public static final long STORAGE_MISS_THRESHOLD_DEFAULT = 100;

    private static final String TRIGGER_IMMEDIATE_PAGINATION = "trigger_immediate_pagination";
    /** Default value for triggering immediate pagination. */
    public static final boolean TRIGGER_IMMEDIATE_PAGINATION_DEFAULT = false;

    private static final String UNDOABLE_ACTIONS_ENABLED = "undoable_actions_enabled";
    /** Default value for if undoable actions should be presented to the user. */
    public static final boolean UNDOABLE_ACTIONS_ENABLED_DEFAULT = true;

    private static final String USE_SECONDARY_PAGE_REQUEST = "use_secondary_page_request";
    /** Default value for pagination behavior. */
    public static final boolean USE_SECONDARY_PAGE_REQUEST_DEFAULT = false;

    private static final String USE_TIMEOUT_SCHEDULER = "use_timeout_scheduler";
    /** Default value for the type of scheduler handling. */
    public static final boolean USE_TIMEOUT_SCHEDULER_DEFAULT = true;

    private static final String VIEW_LOG_THRESHOLD = "view_log_threshold";
    /** Default value for logging view threshold. */
    public static final double VIEW_LOG_THRESHOLD_DEFAULT = 0.66d;

    /** @return Whether the Stream aborts restores if user is past configured fold count. */
    @VisibleForTesting
    static boolean getAbandonRestoreBelowFold() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, ABANDON_RESTORE_BELOW_FOLD,
                ABANDON_RESTORE_BELOW_FOLD_DEFAULT);
    }

    /** @return Whether to show card tooltips. */
    @VisibleForTesting
    static boolean getCardMenuTooltipEligible() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, CARD_MENU_TOOLTIP_ELIGIBLE,
                CARD_MENU_TOOLTIP_ELIGIBLE_DEFAULT);
    }

    /** @return Whether synthetic tokens should be consumed when they are found. */
    @VisibleForTesting
    static boolean getConsumeSyntheticTokens() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, CONSUME_SYNTHETIC_TOKENS,
                CONSUME_SYNTHETIC_TOKENS_DEFAULT);
    }

    /**
     * @return Whether synthetic tokens should be automatically consume when restoring. This will
     * not cause synthetic tokens to be consumed when opening with a new session.
     */
    @VisibleForTesting
    static boolean getConsumeSyntheticTokensWhileRestoring() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING,
                CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING_DEFAULT);
    }

    /** @return The TTL (in seconds) for the default action. */
    @VisibleForTesting
    static long getDefaultActionTtlSeconds() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, DEFAULT_ACTION_TTL_SECONDS,
                (int) DEFAULT_ACTION_TTL_SECONDS_DEFAULT);
    }

    /** @return The endpoint used for recording uploaded actions to the server. */
    @VisibleForTesting
    static String getFeedActionServerEndpoint() {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_ACTION_SERVER_ENDPOINT);
        return TextUtils.isEmpty(paramValue) ? FEED_ACTION_SERVER_ENDPOINT_DEFAULT : paramValue;
    }

    /**
     * @return Maximum number of actions to be uploaded to the endpoint in a single request.
     */
    @VisibleForTesting
    static long getFeedActionServerMaxActionsPerRequest() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST,
                (int) FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST_DEFAULT);
    }

    /**
     * @return Maximum size in bytes of the request to be uploaded to the endpoint in a single
     *         request.
     */
    @VisibleForTesting
    static long getFeedActionServerMaxSizePerRequest() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST,
                (int) FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST_DEFAULT);
    }

    /** @return The HTTP method call to the feed action server (put/post/etc). */
    @VisibleForTesting
    static String getFeedActionServerMethod() {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_ACTION_SERVER_METHOD);
        return TextUtils.isEmpty(paramValue) ? FEED_ACTION_SERVER_METHOD_DEFAULT : paramValue;
    }

    /** @return Feed server endpoint to use to fetch content suggestions. */
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
    static boolean getFeedServerResponseLengthPrefixed() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                FEED_SERVER_RESPONSE_LENGTH_PREFIXED, FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT);
    }

    /** @return Whether to ask the server for "Feed" UI or just basic UI. */
    public static boolean getFeedUiEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_UI_ENABLED,
                FEED_UI_ENABLED_DEFAULT);
    }

    /** @return Used to decide where to place the more button initially. */
    @VisibleForTesting
    static int getInitialNonCachedPageSize() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, INITIAL_NON_CACHED_PAGE_SIZE,
                (int) INITIAL_NON_CACHED_PAGE_SIZE_DEFAULT);
    }

    /** @return Whether to update HEAD when making a page request. */
    @VisibleForTesting
    static boolean getLimitPageUpdatesInHead() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, LIMIT_PAGE_UPDATES_IN_HEAD,
                LIMIT_PAGE_UPDATES_IN_HEAD_DEFAULT);
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
                (int) LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT);
    }

    /** @return Whether to show context menu option to launch to customization page. */
    @VisibleForTesting
    static boolean getManageInterestsEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, MANAGE_INTERESTS_ENABLED,
                MANAGE_INTERESTS_ENABLED_DEFAULT);
    }

    /** @return The maximum number of times that the GC task can re-enqueue itself. */
    @VisibleForTesting
    static long getMaximumGcAttempts() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, MAXIMUM_GC_ATTEMPTS,
                (int) MAXIMUM_GC_ATTEMPTS_DEFAULT);
    }

    /** @return Used to decide where to place the more button. */
    @VisibleForTesting
    static int getNonCachedMinPageSize() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, NON_CACHED_MIN_PAGE_SIZE,
                (int) NON_CACHED_MIN_PAGE_SIZE_DEFAULT);
    }

    /** @return Used to decide where to place the more button. */
    @VisibleForTesting
    static int getNonCachedPageSize() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, NON_CACHED_PAGE_SIZE,
                (int) NON_CACHED_PAGE_SIZE_DEFAULT);
    }

    /** @return Time until feed stops restoring the UI. */
    @VisibleForTesting
    static long getSessionLifetimeMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SESSION_LIFETIME_MS,
                (int) SESSION_LIFETIME_MS_DEFAULT);
    }

    /** @return Whether the article snippets feature is enabled. */
    @VisibleForTesting
    static boolean getSnippetsEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SNIPPETS_ENABLED,
                SNIPPETS_ENABLED_DEFAULT);
    }

    /** @return Delay before a spinner should be shown after content is requested. */
    @VisibleForTesting
    static long getSpinnerDelayMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SPINNER_DELAY_MS,
                (int) SPINNER_DELAY_MS_DEFAULT);
    }

    /** @return Minimum time before a spinner should show before disappearing. */
    @VisibleForTesting
    static long getSpinnerMinimumShowTimeMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SPINNER_MINIMUM_SHOW_TIME_MS,
                (int) SPINNER_MINIMUM_SHOW_TIME_MS_DEFAULT);
    }

    /** @return The number of items that can be missing from a call to FeedStore before failing. */
    @VisibleForTesting
    static long getStorageMissThreshold() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, STORAGE_MISS_THRESHOLD,
                (int) STORAGE_MISS_THRESHOLD_DEFAULT);
    }

    /**
     * @return Whether UI initially shows "More" button upon reaching the end of known content,
     *         when server could potentially have more content.
     */
    @VisibleForTesting
    static boolean getTriggerImmediatePagination() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, TRIGGER_IMMEDIATE_PAGINATION,
                TRIGGER_IMMEDIATE_PAGINATION_DEFAULT);
    }

    /** @return Whether to allow the present the user with the ability to undo actions. */
    @VisibleForTesting
    static boolean getUndoableActionsEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, UNDOABLE_ACTIONS_ENABLED,
                UNDOABLE_ACTIONS_ENABLED_DEFAULT);
    }

    /**
     * @return Whether the Feed's session handling should use logic to deal with timeouts and
     * placing new results below the fold.
     */
    @VisibleForTesting
    static boolean getUseTimeoutScheduler() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, USE_TIMEOUT_SCHEDULER,
                USE_TIMEOUT_SCHEDULER_DEFAULT);
    }

    /**
     * @return If secondary (a more intuitive) pagination approach should be used, or the original
     * Zine matching behavior should be used.
     */
    @VisibleForTesting
    static boolean getUseSecondaryPageRequest() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, USE_SECONDARY_PAGE_REQUEST,
                USE_SECONDARY_PAGE_REQUEST_DEFAULT);
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
                .put(ConfigKey.ABANDON_RESTORE_BELOW_FOLD,
                        FeedConfiguration.getAbandonRestoreBelowFold())
                .put(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE,
                        FeedConfiguration.getCardMenuTooltipEligible())
                .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS,
                        FeedConfiguration.getConsumeSyntheticTokens())
                .put(ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING,
                        FeedConfiguration.getConsumeSyntheticTokensWhileRestoring())
                .put(ConfigKey.DEFAULT_ACTION_TTL_SECONDS,
                        FeedConfiguration.getDefaultActionTtlSeconds())
                .put(ConfigKey.FEED_ACTION_SERVER_ENDPOINT,
                        FeedConfiguration.getFeedActionServerEndpoint())
                .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST,
                        FeedConfiguration.getFeedActionServerMaxActionsPerRequest())
                .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST,
                        FeedConfiguration.getFeedActionServerMaxSizePerRequest())
                .put(ConfigKey.FEED_ACTION_SERVER_METHOD,
                        FeedConfiguration.getFeedActionServerMethod())
                .put(ConfigKey.FEED_SERVER_ENDPOINT, FeedConfiguration.getFeedServerEndpoint())
                .put(ConfigKey.FEED_SERVER_METHOD, FeedConfiguration.getFeedServerMethod())
                .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED,
                        FeedConfiguration.getFeedServerResponseLengthPrefixed())
                .put(ConfigKey.FEED_UI_ENABLED, FeedConfiguration.getFeedUiEnabled())
                .put(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE,
                        FeedConfiguration.getInitialNonCachedPageSize())
                .put(ConfigKey.LIMIT_PAGE_UPDATES_IN_HEAD,
                        FeedConfiguration.getLimitPageUpdatesInHead())
                .put(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                        FeedConfiguration.getLoggingImmediateContentThresholdMs())
                .put(ConfigKey.MANAGE_INTERESTS_ENABLED,
                        FeedConfiguration.getManageInterestsEnabled())
                .put(ConfigKey.MAXIMUM_GC_ATTEMPTS, FeedConfiguration.getMaximumGcAttempts())
                .put(ConfigKey.NON_CACHED_MIN_PAGE_SIZE,
                        FeedConfiguration.getNonCachedMinPageSize())
                .put(ConfigKey.NON_CACHED_PAGE_SIZE, FeedConfiguration.getNonCachedPageSize())
                .put(ConfigKey.SESSION_LIFETIME_MS, FeedConfiguration.getSessionLifetimeMs())
                .put(ConfigKey.SNIPPETS_ENABLED, FeedConfiguration.getSnippetsEnabled())
                .put(ConfigKey.SPINNER_DELAY_MS, FeedConfiguration.getSpinnerDelayMs())
                .put(ConfigKey.SPINNER_MINIMUM_SHOW_TIME_MS,
                        FeedConfiguration.getSpinnerMinimumShowTimeMs())
                .put(ConfigKey.STORAGE_MISS_THRESHOLD, FeedConfiguration.getStorageMissThreshold())
                .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION,
                        FeedConfiguration.getTriggerImmediatePagination())
                .put(ConfigKey.UNDOABLE_ACTIONS_ENABLED,
                        FeedConfiguration.getUndoableActionsEnabled())
                .put(ConfigKey.USE_TIMEOUT_SCHEDULER, FeedConfiguration.getUseTimeoutScheduler())
                .put(ConfigKey.USE_SECONDARY_PAGE_REQUEST,
                        FeedConfiguration.getUseSecondaryPageRequest())
                .put(ConfigKey.VIEW_LOG_THRESHOLD, FeedConfiguration.getViewLogThreshold())
                .build();
    }
}
