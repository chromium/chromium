// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Commerce Subscriptions Service. */
public class CommerceSubscriptionsServiceConfig {
    private static final String BASE_URL_PARAM = "subscriptions_service_base_url";
    private static final String DEFAULT_BASE_URL =
            "https://memex-pa.googleapis.com/v1/shopping/subscriptions";

    private static final String STALE_TAB_LOWER_BOUND_SECONDS_PARAM =
            "price_tracking_stale_tab_lower_bound_seconds";

    public static final StringCachedFieldTrialParameter SUBSCRIPTIONS_SERVICE_BASE_URL =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, BASE_URL_PARAM, DEFAULT_BASE_URL);

    public static final IntCachedFieldTrialParameter STALE_TAB_LOWER_BOUND_SECONDS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                    STALE_TAB_LOWER_BOUND_SECONDS_PARAM, (int) TimeUnit.DAYS.toSeconds(1));
}
