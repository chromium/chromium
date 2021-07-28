// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Merchant Viewer experience. */
public class MerchantViewerConfig {
    private static final String TRUST_SIGNALS_MESSAGE_DELAY_PARAM =
            "trust_signals_message_delay_ms";
    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM =
            "trust_signals_message_window_duration_ms";
    @VisibleForTesting
    public static final String TRUST_SIGNALS_SHEET_USE_PAGE_TITLE_PARAM =
            "trust_signals_sheet_use_page_title";
    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM =
            "trust_signals_message_use_rating_bar";
    @VisibleForTesting
    public static final String TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM =
            "trust_signals_use_site_engagement";
    @VisibleForTesting
    public static final String TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM =
            "trust_signals_site_engagement_threshold";

    public static int getDefaultTrustSignalsMessageDelay() {
        int defaultDelay = (int) TimeUnit.SECONDS.toMillis(30);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER, TRUST_SIGNALS_MESSAGE_DELAY_PARAM,
                    defaultDelay);
        }
        return defaultDelay;
    }

    public static int getTrustSignalsMessageWindowDurationSeconds() {
        int defaultDuration = (int) TimeUnit.DAYS.toMillis(365);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM, defaultDuration);
        }
        return defaultDuration;
    }

    public static boolean doesTrustSignalsSheetUsePageTitle() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_SHEET_USE_PAGE_TITLE_PARAM, true);
        }
        return true;
    }

    public static boolean doesTrustSignalsMessageUseRatingBar() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM, true);
        }
        return true;
    }

    public static boolean doesTrustSignalsUseSiteEngagement() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM, true);
        }
        return true;
    }

    public static double getTrustSignalsSiteEngagementThreshold() {
        double defaultThreshold = 90.0;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM, defaultThreshold);
        }
        return defaultThreshold;
    }
}
