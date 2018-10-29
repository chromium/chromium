// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
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
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_ENDPOINT_DEFAULT,
                FeedConfiguration.getFeedServerEndpoint());
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_METHOD_DEFAULT,
                FeedConfiguration.getFeedServerMethod());
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT,
                FeedConfiguration.getFeedServerReponseLengthPrefixed());
        Assert.assertEquals(FeedConfiguration.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT,
                FeedConfiguration.getLoggingImmediateContentThresholdMs());
        Assert.assertEquals(FeedConfiguration.SESSION_LIFETIME_MS_DEFAULT,
                FeedConfiguration.getSessionLifetimeMs());
        Assert.assertFalse(FeedConfiguration.getTriggerImmedatePagination());
        Assert.assertEquals(FeedConfiguration.VIEW_LOG_THRESHOLD_DEFAULT,
                FeedConfiguration.getViewLogThreshold(), ASSERT_EQUALS_DOUBLE_DELTA);
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_server_endpoint/"
                    + "https%3A%2F%2Ffeed%2Egoogle%2Ecom%2Fpath"})
    public void testFeedServerEndpointParameter() {
        Assert.assertEquals(
                "https://feed.google.com/path", FeedConfiguration.getFeedServerEndpoint());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_server_method/POST"})
    public void testFeedServerMethodParameter() {
        Assert.assertEquals("POST", FeedConfiguration.getFeedServerMethod());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:feed_server_response_length_prefixed/false"})
    public void testFeedServerResponseLengthPrefixedParameter() {
        Assert.assertEquals(false, FeedConfiguration.getFeedServerReponseLengthPrefixed());
    }

    @Test
    @Feature({"Feed"})
    @CommandLineFlags.
    Add({"enable-features=InterestFeedContentSuggestions<Trial", "force-fieldtrials=Trial/Group",
            "force-fieldtrial-params=Trial.Group:logging_immediate_content_threshold_ms/5000"})
    public void testLoggingImmediateContentThresholdMs() {
        Assert.assertEquals(5000, FeedConfiguration.getLoggingImmediateContentThresholdMs());
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
            "force-fieldtrial-params=Trial.Group:trigger_immediate_pagination/true"})
    public void testTriggerImmedatePagination() {
        Assert.assertTrue(FeedConfiguration.getTriggerImmedatePagination());
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
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_ENDPOINT_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_SERVER_ENDPOINT, ""));
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_METHOD_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_SERVER_METHOD, ""));
        Assert.assertEquals(FeedConfiguration.FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT,
                configuration.getValueOrDefault(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED, 0));
        Assert.assertEquals(
                Long.valueOf(FeedConfiguration.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT),
                configuration.getValueOrDefault(
                        ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS, 0l));
        Assert.assertEquals(Long.valueOf(FeedConfiguration.SESSION_LIFETIME_MS_DEFAULT),
                configuration.getValueOrDefault(ConfigKey.SESSION_LIFETIME_MS, 0l));
        Assert.assertFalse(
                configuration.getValueOrDefault(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION, true));
        Assert.assertEquals(Double.valueOf(FeedConfiguration.VIEW_LOG_THRESHOLD_DEFAULT),
                configuration.getValueOrDefault(ConfigKey.VIEW_LOG_THRESHOLD, 0d),
                ASSERT_EQUALS_DOUBLE_DELTA);
    }
}
