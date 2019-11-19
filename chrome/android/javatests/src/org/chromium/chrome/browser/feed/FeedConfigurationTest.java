// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link FeedConfiguration}. */
@SmallTest
@RunWith(ChromeJUnit4ClassRunner.class)
public class FeedConfigurationTest {
    @Rule
    public final ChromeBrowserTestRule mRule = new ChromeBrowserTestRule();

    private static final double ASSERT_EQUALS_DOUBLE_DELTA = 0.001d;

    @Test
    @Feature({"Feed"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS})
    public void testDefaultFeedConfigurationValues() {
        Assert.assertEquals(FeedConfiguration.ABANDON_RESTORE_BELOW_FOLD_DEFAULT,
                FeedConfiguration.getAbandonRestoreBelowFold());
        Assert.assertEquals(FeedConfiguration.CARD_MENU_TOOLTIP_ELIGIBLE_DEFAULT,
                FeedConfiguration.getCardMenuTooltipEligible());
        Assert.assertEquals(FeedConfiguration.CONSUME_SYNTHETIC_TOKENS_DEFAULT,
                FeedConfiguration.getConsumeSyntheticTokens());
        Assert.assertEquals(FeedConfiguration.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING_DEFAULT,
                FeedConfiguration.getConsumeSyntheticTokensWhileRestoring());
        Assert.assertEquals(FeedConfiguration.DEFAULT_ACTION_TTL_SECONDS_DEFAULT,
                FeedConfiguration.getDefaultActionTtlSeconds());
        Assert.assertEquals(FeedConfiguration.FEED_ACTION_SERVER_ENDPOINT_DEFAULT,
                FeedConfiguration.getFeedActionServerEndpoint());
        Assert.assertEquals(FeedConfiguration.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST_DEFAULT,
                FeedConfiguration.getFeedActionServerMaxActionsPerRequest());
        Assert.assertEquals(FeedConfiguration.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST_DEFAULT,
                FeedConfiguration.getFeedActionServerMaxSizePerRequest());
        Assert.assertEquals(FeedConfiguration.FEED_ACTION_SERVER_METHOD_DEFAULT,
                FeedConfiguration.getFeedActionServerMethod());
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_ENDPOINT_DEFAULT,
                FeedConfiguration.getFeedServerEndpoint());
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_METHOD_DEFAULT,
                FeedConfiguration.getFeedServerMethod());
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT,
                FeedConfiguration.getFeedServerResponseLengthPrefixed());
        Assert.assertEquals(
                FeedConfiguration.FEED_UI_ENABLED_DEFAULT, FeedConfiguration.getFeedUiEnabled());
        Assert.assertEquals(FeedConfiguration.INITIAL_NON_CACHED_PAGE_SIZE_DEFAULT,
                FeedConfiguration.getInitialNonCachedPageSize());
        Assert.assertEquals(FeedConfiguration.LIMIT_PAGE_UPDATES_IN_HEAD_DEFAULT,
                FeedConfiguration.getLimitPageUpdatesInHead());
        Assert.assertEquals(FeedConfiguration.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT,
                FeedConfiguration.getLoggingImmediateContentThresholdMs());
        Assert.assertEquals(FeedConfiguration.MANAGE_INTERESTS_ENABLED_DEFAULT,
                FeedConfiguration.getManageInterestsEnabled());
        Assert.assertEquals(FeedConfiguration.MAXIMUM_GC_ATTEMPTS_DEFAULT,
                FeedConfiguration.getMaximumGcAttempts());
        Assert.assertEquals(FeedConfiguration.NON_CACHED_MIN_PAGE_SIZE_DEFAULT,
                FeedConfiguration.getNonCachedMinPageSize());
        Assert.assertEquals(FeedConfiguration.NON_CACHED_PAGE_SIZE_DEFAULT,
                FeedConfiguration.getNonCachedPageSize());
        Assert.assertEquals(FeedConfiguration.SESSION_LIFETIME_MS_DEFAULT,
                FeedConfiguration.getSessionLifetimeMs());
        Assert.assertEquals(
                FeedConfiguration.SNIPPETS_ENABLED_DEFAULT, FeedConfiguration.getSnippetsEnabled());
        Assert.assertEquals(
                FeedConfiguration.SPINNER_DELAY_MS_DEFAULT, FeedConfiguration.getSpinnerDelayMs());
        Assert.assertEquals(FeedConfiguration.SPINNER_MINIMUM_SHOW_TIME_MS_DEFAULT,
                FeedConfiguration.getSpinnerMinimumShowTimeMs());
        Assert.assertEquals(FeedConfiguration.STORAGE_MISS_THRESHOLD_DEFAULT,
                FeedConfiguration.getStorageMissThreshold());
        Assert.assertEquals(FeedConfiguration.TRIGGER_IMMEDIATE_PAGINATION_DEFAULT,
                FeedConfiguration.getTriggerImmediatePagination());
        Assert.assertEquals(FeedConfiguration.UNDOABLE_ACTIONS_ENABLED_DEFAULT,
                FeedConfiguration.getUndoableActionsEnabled());
        Assert.assertEquals(FeedConfiguration.USE_TIMEOUT_SCHEDULER_DEFAULT,
                FeedConfiguration.getUseTimeoutScheduler());
        Assert.assertEquals(FeedConfiguration.USE_SECONDARY_PAGE_REQUEST_DEFAULT,
                FeedConfiguration.getUseSecondaryPageRequest());
        Assert.assertEquals(FeedConfiguration.VIEW_LOG_THRESHOLD_DEFAULT,
                FeedConfiguration.getViewLogThreshold(), ASSERT_EQUALS_DOUBLE_DELTA);
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:abandon_restore_below_fold/false"})
    public void
    testAbandonRestoreBelowFold() {
        Assert.assertFalse(FeedConfiguration.getAbandonRestoreBelowFold());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:card_menu_tooltip_eligible/true"})
    public void
    testCardMenuTooltipEligible() {
        Assert.assertTrue(FeedConfiguration.getCardMenuTooltipEligible());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:consume_synthetic_tokens_bool/true"})
    public void
    testConsumeSyntheticTokens() {
        Assert.assertTrue(FeedConfiguration.getConsumeSyntheticTokens());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:consume_synthetic_tokens_while_restoring_bool/"
                    + "true"})
    public void
    testConsumeSyntheticTokensWhileRestoring() {
        Assert.assertTrue(FeedConfiguration.getConsumeSyntheticTokensWhileRestoring());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:default_action_ttl_seconds/42"})
    public void
    testDefaultActionTTLSeconds() {
        Assert.assertEquals(42, FeedConfiguration.getDefaultActionTtlSeconds());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_action_server_endpoint/"
                    + "https%3A%2F%2Ffeed%2Egoogle%2Ecom%2Fpath"})
    public void
    testFeedActionServerEndpoint() {
        Assert.assertEquals(
                "https://feed.google.com/path", FeedConfiguration.getFeedActionServerEndpoint());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_action_server_max_actions_per_request/1234"})
    public void
    testFeedActionServerMaxActionsPerRequest() {
        Assert.assertEquals(1234, FeedConfiguration.getFeedActionServerMaxActionsPerRequest());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_action_server_max_size_per_request/1234"})
    public void
    testFeedActionServerMaxSizePerRequest() {
        Assert.assertEquals(1234, FeedConfiguration.getFeedActionServerMaxSizePerRequest());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_action_server_method/PUT"})
    public void
    testFeedActionServerMethod() {
        Assert.assertEquals("PUT", FeedConfiguration.getFeedActionServerMethod());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_server_endpoint/"
                    + "https%3A%2F%2Ffeed%2Egoogle%2Ecom%2Fpath"})
    public void
    testFeedServerEndpoint() {
        Assert.assertEquals(
                "https://feed.google.com/path", FeedConfiguration.getFeedServerEndpoint());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_server_method/POST"})
    public void
    testFeedServerMethod() {
        Assert.assertEquals("POST", FeedConfiguration.getFeedServerMethod());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_server_response_length_prefixed/false"})
    public void
    testFeedServerResponseLengthPrefixed() {
        Assert.assertEquals(false, FeedConfiguration.getFeedServerResponseLengthPrefixed());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_ui_enabled/true"})
    public void
    testFeedUiEnabled() {
        Assert.assertTrue(FeedConfiguration.getFeedUiEnabled());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:initial_non_cached_page_size/100"})
    public void
    testInitialNonCachedPageSize() {
        Assert.assertEquals(100, FeedConfiguration.getInitialNonCachedPageSize());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:limit_page_updates_in_head/true"})
    public void
    testLimitPageUpdatesInHead() {
        Assert.assertTrue(FeedConfiguration.getLimitPageUpdatesInHead());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:logging_immediate_content_threshold_ms/5000"})
    public void
    testLoggingImmediateContentThresholdMs() {
        Assert.assertEquals(5000, FeedConfiguration.getLoggingImmediateContentThresholdMs());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:manage_interests_enabled/true"})
    public void
    testManageInterestsEnabled() {
        Assert.assertTrue(FeedConfiguration.getManageInterestsEnabled());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:maximum_gc_attempts/5"})
    public void
    testMaximumGcAttempts() {
        Assert.assertEquals(5, FeedConfiguration.getMaximumGcAttempts());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:non_cached_min_page_size/100"})
    public void
    testNonCachedMinPageSize() {
        Assert.assertEquals(100, FeedConfiguration.getNonCachedMinPageSize());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:non_cached_page_size/100"})
    public void
    testNonCachedPageSize() {
        Assert.assertEquals(100, FeedConfiguration.getNonCachedPageSize());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:session_lifetime_ms/60000"})
    public void testSessionLifetimeMs() {
        Assert.assertEquals(60000, FeedConfiguration.getSessionLifetimeMs());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:snippets_enabled/true"})
    public void
    testSnippetsEnabled() {
        Assert.assertTrue(FeedConfiguration.getSnippetsEnabled());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:spinner_delay/333"})
    public void
    testSpinnerDelayMs() {
        Assert.assertEquals(333, FeedConfiguration.getSpinnerDelayMs());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:storage_miss_threshold/444"})
    public void
    testStorageMissThreshold() {
        Assert.assertEquals(444, FeedConfiguration.getStorageMissThreshold());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:spinner_minimum_show_time/444"})
    public void
    testSpinnerMinimumShowTimeMs() {
        Assert.assertEquals(444, FeedConfiguration.getSpinnerMinimumShowTimeMs());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:trigger_immediate_pagination/true"})
    public void
    testTriggerImmedatePagination() {
        Assert.assertTrue(FeedConfiguration.getTriggerImmediatePagination());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:undoable_actions_enabled/true"})
    public void
    testUndoableActionsEnabled() {
        Assert.assertTrue(FeedConfiguration.getUndoableActionsEnabled());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:use_timeout_scheduler/false"})
    public void
    testUseTimeoutScheduler() {
        Assert.assertFalse(FeedConfiguration.getUseTimeoutScheduler());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:use_secondary_page_request/true"})
    public void
    testUseSecondaryPageRequest() {
        Assert.assertTrue(FeedConfiguration.getUseSecondaryPageRequest());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:view_log_threshold/0.33"})
    public void testViewLogThreshold() {
        Assert.assertEquals(
                0.33d, FeedConfiguration.getViewLogThreshold(), ASSERT_EQUALS_DOUBLE_DELTA);
    }

    @Test
    @Feature({"Feed"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS})
    public void testCreateConfiguration() {
        Configuration configuration = FeedConfiguration.createConfiguration();
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.ABANDON_RESTORE_BELOW_FOLD, true));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE, true));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.CONSUME_SYNTHETIC_TOKENS, true));
        Assert.assertTrue(configuration.getValueOrDefault(
                ConfigKey.CONSUME_SYNTHETIC_TOKENS_WHILE_RESTORING, false));
        Assert.assertEquals((long) FeedConfiguration.DEFAULT_ACTION_TTL_SECONDS_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.DEFAULT_ACTION_TTL_SECONDS, 0));
        Assert.assertEquals(FeedConfiguration.FEED_ACTION_SERVER_ENDPOINT_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_ENDPOINT, ""));
        Assert.assertEquals(
                (long) FeedConfiguration.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST_DEFAULT,
                configuration.getValueOrDefault(
                        ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 0));
        Assert.assertEquals(
                (long) FeedConfiguration.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST_DEFAULT,
                configuration.getValueOrDefault(
                        ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 0));
        Assert.assertEquals(FeedConfiguration.FEED_ACTION_SERVER_METHOD_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_METHOD, ""));
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_ENDPOINT_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_SERVER_ENDPOINT, ""));
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_METHOD_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_SERVER_METHOD, ""));
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT,
                configuration.getValueOrDefault(
                        ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, false));
        Assert.assertFalse(configuration.getValueOrDefault(ConfigKey.FEED_UI_ENABLED, true));
        Assert.assertEquals((long) FeedConfiguration.INITIAL_NON_CACHED_PAGE_SIZE_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE, 0));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.LIMIT_PAGE_UPDATES_IN_HEAD, true));
        Assert.assertEquals((long) FeedConfiguration.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT,
                configuration.getValueOrDefault(
                        ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS, 0l));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.MANAGE_INTERESTS_ENABLED, true));
        Assert.assertEquals((long) FeedConfiguration.MAXIMUM_GC_ATTEMPTS_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.MAXIMUM_GC_ATTEMPTS, 0l));
        Assert.assertEquals((long) FeedConfiguration.NON_CACHED_MIN_PAGE_SIZE_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.NON_CACHED_MIN_PAGE_SIZE, 0));
        Assert.assertEquals((long) FeedConfiguration.NON_CACHED_PAGE_SIZE_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.NON_CACHED_PAGE_SIZE, 0));
        Assert.assertEquals((long) FeedConfiguration.SESSION_LIFETIME_MS_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.SESSION_LIFETIME_MS, 0l));
        Assert.assertFalse(configuration.getValueOrDefault(ConfigKey.SNIPPETS_ENABLED, true));
        Assert.assertEquals((long) FeedConfiguration.SPINNER_DELAY_MS_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.SPINNER_DELAY_MS, 0l));
        Assert.assertEquals((long) FeedConfiguration.SPINNER_MINIMUM_SHOW_TIME_MS_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.SPINNER_MINIMUM_SHOW_TIME_MS, 0l));
        Assert.assertEquals((long) FeedConfiguration.STORAGE_MISS_THRESHOLD_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.STORAGE_MISS_THRESHOLD, 0l));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.UNDOABLE_ACTIONS_ENABLED, true));
        Assert.assertTrue(configuration.getValueOrDefault(ConfigKey.USE_TIMEOUT_SCHEDULER, false));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.USE_SECONDARY_PAGE_REQUEST, true));
        Assert.assertEquals(Double.valueOf(FeedConfiguration.VIEW_LOG_THRESHOLD_DEFAULT),
                configuration.getValueOrDefault(ConfigKey.VIEW_LOG_THRESHOLD, 0d),
                ASSERT_EQUALS_DOUBLE_DELTA);
    }
}
