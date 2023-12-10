// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMessageViewModel.MessageDescriptionUI;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMessageViewModel.MessageTitleUI;

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

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MAX_ALLOWED_NUMBER_IN_GIVEN_WINDOW_PARAM =
            "trust_signals_max_allowed_number_in_given_window";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_NUMBER_CHECK_WINDOW_DURATION_PARAM =
            "trust_signals_number_check_window_duration_ms";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_DISABLED_PARAM =
            "trust_signals_message_disabled";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_RATING_THRESHOLD_PARAM =
            "trust_signals_message_rating_threshold";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_NON_PERSONALIZED_FAMILIARITY_SCORE_THRESHOLD_PARAM =
            "trust_signals_non_personalized_familiarity_score_threshold";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_USE_GOOGLE_ICON_PARAM =
            "trust_signals_message_use_google_icon";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_TITLE_UI_PARAM =
            "trust_signals_message_title_ui";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_DESCRIPTION_UI_PARAM =
            "trust_signals_message_description_ui";

    @VisibleForTesting
    public static final String TRUST_SIGNALS_MESSAGE_DISABLED_FOR_IMPACT_STUDY_PARAM =
            "trust_signals_message_disabled_for_impact_study";

    public static int getDefaultTrustSignalsMessageDelay() {
        int defaultDelay = (int) TimeUnit.SECONDS.toMillis(30);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_DELAY_PARAM,
                    defaultDelay);
        }
        return defaultDelay;
    }

    public static int getTrustSignalsMessageWindowDurationMilliSeconds() {
        int defaultDuration = (int) TimeUnit.DAYS.toMillis(365);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_WINDOW_DURATION_PARAM,
                    defaultDuration);
        }
        return defaultDuration;
    }

    public static boolean doesTrustSignalsSheetUsePageTitle() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_SHEET_USE_PAGE_TITLE_PARAM,
                    true);
        }
        return true;
    }

    public static boolean doesTrustSignalsMessageUseRatingBar() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_USE_RATING_BAR_PARAM,
                    true);
        }
        return true;
    }

    public static boolean doesTrustSignalsUseSiteEngagement() {
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_USE_SITE_ENGAGEMENT_PARAM,
                    true);
        }
        return true;
    }

    public static double getTrustSignalsSiteEngagementThreshold() {
        double defaultThreshold = 90.0;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_SITE_ENGAGEMENT_THRESHOLD_PARAM,
                    defaultThreshold);
        }
        return defaultThreshold;
    }

    public static int getTrustSignalsMaxAllowedNumberInGivenWindow() {
        int defaultMaxAllowedNumber = 3;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MAX_ALLOWED_NUMBER_IN_GIVEN_WINDOW_PARAM,
                    defaultMaxAllowedNumber);
        }
        return defaultMaxAllowedNumber;
    }

    public static int getTrustSignalsNumberCheckWindowDuration() {
        int defaultDuration = (int) TimeUnit.HOURS.toMillis(1);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_NUMBER_CHECK_WINDOW_DURATION_PARAM,
                    defaultDuration);
        }
        return defaultDuration;
    }

    public static boolean isTrustSignalsMessageDisabled() {
        boolean defaultValue = true;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_DISABLED_PARAM,
                    defaultValue);
        }
        return defaultValue;
    }

    public static double getTrustSignalsMessageRatingThreshold() {
        double defaultThreshold = 4.0;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_RATING_THRESHOLD_PARAM,
                    defaultThreshold);
        }
        return defaultThreshold;
    }

    public static double getTrustSignalsNonPersonalizedFamiliarityScoreThreshold() {
        double defaultThreshold = 0.8;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_NON_PERSONALIZED_FAMILIARITY_SCORE_THRESHOLD_PARAM,
                    defaultThreshold);
        }
        return defaultThreshold;
    }

    public static boolean doesTrustSignalsMessageUseGoogleIcon() {
        boolean defaultValue = false;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_USE_GOOGLE_ICON_PARAM,
                    defaultValue);
        }
        return defaultValue;
    }

    public static int getTrustSignalsMessageTitleUI() {
        int defaultUI = MessageTitleUI.VIEW_STORE_INFO;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_TITLE_UI_PARAM,
                    defaultUI);
        }
        return defaultUI;
    }

    public static int getTrustSignalsMessageDescriptionUI() {
        int defaultUI = MessageDescriptionUI.RATING_AND_REVIEWS;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_DESCRIPTION_UI_PARAM,
                    defaultUI);
        }
        return defaultUI;
    }

    public static boolean isTrustSignalsMessageDisabledForImpactStudy() {
        boolean defaultValue = false;
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                    TRUST_SIGNALS_MESSAGE_DISABLED_FOR_IMPACT_STUDY_PARAM,
                    defaultValue);
        }
        return defaultValue;
    }
}
