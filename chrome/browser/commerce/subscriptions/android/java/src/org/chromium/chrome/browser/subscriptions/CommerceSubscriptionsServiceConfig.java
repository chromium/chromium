// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Commerce Subscriptions Service. */
public class CommerceSubscriptionsServiceConfig {
    @VisibleForTesting
    private static final String STALE_TAB_LOWER_BOUND_SECONDS_PARAM =
            "price_tracking_stale_tab_lower_bound_seconds";

    private static final int DEFAULT_STALE_TAB_LOWER_BOUND_DAYS = 1;

    public static int getStaleTabLowerBoundSeconds() {
        int defaultValue = (int) TimeUnit.DAYS.toSeconds(DEFAULT_STALE_TAB_LOWER_BOUND_DAYS);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    STALE_TAB_LOWER_BOUND_SECONDS_PARAM,
                    defaultValue);
        }
        return defaultValue;
    }
}
