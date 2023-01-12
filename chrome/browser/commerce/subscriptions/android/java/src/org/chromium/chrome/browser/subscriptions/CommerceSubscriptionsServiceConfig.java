// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;
/** Flag configuration for Commerce Subscriptions Service. */
public class CommerceSubscriptionsServiceConfig {
    private static final String sServiceBaseUrl =
            "https://memex-pa.googleapis.com/v1/shopping/subscriptions";

    @VisibleForTesting
    private static final String sBaseUrlParam = "subscriptions_service_base_url";

    @VisibleForTesting
    private static final String STALE_TAB_LOWER_BOUND_SECONDS_PARAM =
            "price_tracking_stale_tab_lower_bound_seconds";

    @VisibleForTesting
    public static final String IMPLICIT_SUBSCRIPTIONS_ENABLED_PARAM =
            "implicit_subscriptions_enabled";

    private static final String PARSE_SEEN_OFFER_TO_SERVER_PARAM =
            "price_tracking_parse_seen_offer_to_server";

    private static final int DEFAULT_STALE_TAB_LOWER_BOUND_DAYS = 1;

    public static String getDefaultServiceUrl() {
        String defaultValue = sServiceBaseUrl;
        if (FeatureList.isInitialized()) {
            defaultValue = ChromeFeatureList.getFieldTrialParamByFeature(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, sBaseUrlParam);
        }

        return TextUtils.isEmpty(defaultValue) ? sServiceBaseUrl : defaultValue;
    }

    public static int getStaleTabLowerBoundSeconds() {
        int defaultValue = (int) TimeUnit.DAYS.toSeconds(DEFAULT_STALE_TAB_LOWER_BOUND_DAYS);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, STALE_TAB_LOWER_BOUND_SECONDS_PARAM,
                    defaultValue);
        }
        return defaultValue;
    }

    public static boolean isImplicitSubscriptionsEnabled() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, IMPLICIT_SUBSCRIPTIONS_ENABLED_PARAM,
                    false);
        }
        return false;
    }

    public static boolean shouldParseSeenOfferToServer() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, PARSE_SEEN_OFFER_TO_SERVER_PARAM,
                    true);
        }
        return true;
    }
}
