// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A class to handle the state of flags for Feed position experiment.
 */
public class FeedPositionUtils {
    public static final String PUSH_DOWN_FEED_SMALL = "push_down_feed_small";
    public static final String PUSH_DOWN_FEED_LARGE = "push_down_feed_large";
    public static final String PULL_UP_FEED = "pull_up_feed";

    public static boolean isFeedPushDownSmallEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.FEED_POSITION_ANDROID, PUSH_DOWN_FEED_SMALL, false);
    }

    public static boolean isFeedPushDownLargeEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.FEED_POSITION_ANDROID, PUSH_DOWN_FEED_LARGE, false);
    }

    public static boolean isFeedPullUpEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.FEED_POSITION_ANDROID, PULL_UP_FEED, false);
    }
}
