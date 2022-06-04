// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.variations.VariationsAssociatedData;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides Field Trial support for the Contextual Search application within Chrome for Android.
 */
public class ContextualSearchFieldTrial {
    private static final String FIELD_TRIAL_NAME = "ContextualSearch";
    private static final String DISABLED_PARAM = "disabled";
    private static final String ENABLED_VALUE = "true";

    //==========================================================================================
    // Related Searches FieldTrial and parameter names.
    //==========================================================================================
    // Params used elsewhere but gathered here since they may be present in FieldTrial configs.
    static final String RELATED_SEARCHES_NEEDS_URL_PARAM_NAME = "needs_url";
    static final String RELATED_SEARCHES_NEEDS_CONTENT_PARAM_NAME = "needs_content";
    // A comma-separated list of lower-case ISO 639 language codes.
    static final String RELATED_SEARCHES_LANGUAGE_ALLOWLIST_PARAM_NAME = "language_allowlist";
    static final String RELATED_SEARCHES_LANGUAGE_DEFAULT_ALLOWLIST = "en";
    private static final String RELATED_SEARCHES_CONFIG_STAMP_PARAM_NAME = "stamp";

    static final String RELATED_SEARCHES_SHOW_DEFAULT_QUERY_CHIP_PARAM_NAME = "default_query_chip";
    static final String RELATED_SEARCHES_DEFAULT_QUERY_CHIP_MAX_WIDTH_SP_PARAM_NAME =
            "default_query_max_width_sp";

    static final String CONTEXTUAL_SEARCH_PROMO_CARD_MAX_SHOWN_PARAM_NAME = "promo_card_max_shown";
    private static final int PROMO_DEFAULT_LIMIT = 3;

    // Cached values to avoid repeated and redundant JNI operations.
    private static Boolean sEnabled;
    private static Boolean[] sSwitches = new Boolean[ContextualSearchSwitch.NUM_ENTRIES];

    // SWITCHES
    // TODO(donnd): remove all supporting code once short-lived data collection is done.
    @IntDef({ContextualSearchSwitch.IS_TRANSLATION_DISABLED,
            ContextualSearchSwitch.IS_ONLINE_DETECTION_DISABLED,
            ContextualSearchSwitch.IS_SEARCH_TERM_RESOLUTION_DISABLED,
            ContextualSearchSwitch.IS_MANDATORY_PROMO_ENABLED,
            ContextualSearchSwitch.IS_ENGLISH_TARGET_TRANSLATION_ENABLED,
            ContextualSearchSwitch.IS_BAR_OVERLAP_COLLECTION_ENABLED,
            ContextualSearchSwitch.IS_BAR_OVERLAP_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_WORD_EDGE_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_SHORT_WORD_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_NOT_LONG_WORD_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_NOT_AN_ENTITY_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_ENGAGEMENT_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_SHORT_TEXT_RUN_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_SMALL_TEXT_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_AMP_AS_SEPARATE_TAB_DISABLED,
            ContextualSearchSwitch.IS_SEND_HOME_COUNTRY_DISABLED,
            ContextualSearchSwitch.IS_PAGE_CONTENT_NOTIFICATION_DISABLED,
            ContextualSearchSwitch.IS_UKM_RANKER_LOGGING_DISABLED,
            ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION_ENABLED,
            ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE_ENABLED,
            ContextualSearchSwitch.IS_SEND_BASE_PAGE_URL_DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    /**
     * Boolean Switch values that are backed by either a Feature or a Variations parameter.
     * Values are used for indexing ContextualSearchSwitchNames - should start from 0 and can't
     * have gaps.
     */
    @interface ContextualSearchSwitch {
        /**
         * @deprecated
         * Whether all translate code is disabled (master switch, needed to disable all translate
         * code for Contextual Search in case of an emergency).
         */
        int IS_TRANSLATION_DISABLED = 0;
        /**
         * Whether detection of device-online should be disabled (default false).
         * (safety switch for disabling online-detection also used to disable detection when
         * running tests).
         */
        // TODO(donnd): Convert to test-only after launch and we have confidence it's robust.
        int IS_ONLINE_DETECTION_DISABLED = 1;

        /** @deprecated */
        int IS_SEARCH_TERM_RESOLUTION_DISABLED = 2;
        /** @deprecated */
        int IS_MANDATORY_PROMO_ENABLED = 3;

        /**
         * @deprecated
         * Whether English-target translation should be enabled (default is disabled for 'en').
         * Enables usage of English as the target language even when it's the primary UI language.
         */
        int IS_ENGLISH_TARGET_TRANSLATION_ENABLED = 4;
        /**
         * @deprecated
         * Whether collecting data on Bar overlap is enabled.
         */
        int IS_BAR_OVERLAP_COLLECTION_ENABLED = 5;
        /**
         * @deprecated
         * Whether triggering is suppressed by a selection nearly overlapping the normal
         * Bar peeking location.
         */
        int IS_BAR_OVERLAP_SUPPRESSION_ENABLED = 6;
        /**
         * @deprecated
         * Whether triggering is suppressed by a tap that's near the edge of a word.
         */
        int IS_WORD_EDGE_SUPPRESSION_ENABLED = 7;
        /**
         * @deprecated
         * Whether triggering is suppressed by a tap that's in a short word.
         * */
        int IS_SHORT_WORD_SUPPRESSION_ENABLED = 8;
        /**
         * @deprecated
         * Whether triggering is suppressed by a tap that's not in a long word.
         */
        int IS_NOT_LONG_WORD_SUPPRESSION_ENABLED = 9;
        /**
         * @deprecated
         * Whether triggering is suppressed for a tap that's not on an entity.
         */
        int IS_NOT_AN_ENTITY_SUPPRESSION_ENABLED = 10;
        /**
         * @deprecated
         * Whether triggering is suppressed due to lack of engagement with the feature.
         */
        int IS_ENGAGEMENT_SUPPRESSION_ENABLED = 11;
        /**
         * @deprecated
         * Whether triggering is suppressed for a tap that has a short element run-length.
         */
        int IS_SHORT_TEXT_RUN_SUPPRESSION_ENABLED = 12;
        /**
         * @deprecated
         * Whether triggering is suppressed for a tap on small-looking text.
         */
        int IS_SMALL_TEXT_SUPPRESSION_ENABLED = 13;
        /**
         * @deprecated
         * Whether to disable auto-promotion of clicks in the AMP carousel into a
         * separate Tab.
         */
        int IS_AMP_AS_SEPARATE_TAB_DISABLED = 14;
        /**
         * @deprecated
         * Whether sending the "home country" to Google is disabled.
         */
        int IS_SEND_HOME_COUNTRY_DISABLED = 15;
        /**
         * @deprecated
         * Whether sending the page content notifications to observers (e.g. icing for
         * conversational search) is disabled.
         */
        int IS_PAGE_CONTENT_NOTIFICATION_DISABLED = 16;
        /**
         * @deprecated
         * Whether logging for Machine Learning is disabled.
         */
        int IS_UKM_RANKER_LOGGING_DISABLED = 17;
        /**
         * @deprecated
         * Whether or not ML-based Tap suppression is enabled.
         */
        int IS_CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION_ENABLED = 18;
        /**
         * @deprecated
         * Whether or not to override tap-disable for users that have never opened the
         * panel.
         */
        int IS_CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE_ENABLED = 19;
        /**
         * @deprecated
         * Whether sending the URL of the page viewed by the user is disabled.
         */
        int IS_SEND_BASE_PAGE_URL_DISABLED = 20;

        int NUM_ENTRIES = 21;
    }

    @VisibleForTesting
    static final String ONLINE_DETECTION_DISABLED = "disable_online_detection";
    @VisibleForTesting
    static final String TRANSLATION_DISABLED = "disable_translation";

    // Indexed by ContextualSearchSwitch
    private static final String[] ContextualSearchSwitchNames = {
            TRANSLATION_DISABLED, // IS_TRANSLATION_DISABLED
            ONLINE_DETECTION_DISABLED, // IS_ONLINE_DETECTION_DISABLED
            "disable_search_term_resolution", // DISABLE_SEARCH_TERM_RESOLUTION
            "mandatory_promo_enabled", // IS_MANDATORY_PROMO_ENABLED
            "enable_english_target_translation", // IS_ENGLISH_TARGET_TRANSLATION_ENABLED
            "enable_bar_overlap_collection", // IS_BAR_OVERLAP_COLLECTION_ENABLED
            "enable_bar_overlap_suppression", // IS_BAR_OVERLAP_SUPPRESSION_ENABLED
            "enable_word_edge_suppression", // IS_WORD_EDGE_SUPPRESSION_ENABLED
            "enable_short_word_suppression", //  IS_SHORT_WORD_SUPPRESSION_ENABLED
            "enable_not_long_word_suppression", // IS_NOT_LONG_WORD_SUPPRESSION_ENABLED
            "enable_not_an_entity_suppression", //  IS_NOT_AN_ENTITY_SUPPRESSION_ENABLED
            "enable_engagement_suppression", // IS_ENGAGEMENT_SUPPRESSION_ENABLED
            "enable_short_text_run_suppression", // IS_SHORT_TEXT_RUN_SUPPRESSION_ENABLED
            "enable_small_text_suppression", // IS_SMALL_TEXT_SUPPRESSION_ENABLED
            "disable_amp_as_separate_tab", // IS_AMP_AS_SEPARATE_TAB_DISABLED
            "disable_send_home_country", //  IS_SEND_HOME_COUNTRY_DISABLED
            "disable_page_content_notification", // IS_PAGE_CONTENT_NOTIFICATION_DISABLED
            "disable_ukm_ranker_logging", // IS_UKM_RANKER_LOGGING_DISABLED
            ChromeFeatureList.CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION, // (related to Chrome Feature)
            ChromeFeatureList.CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE, // (related to Chrome Feature)
            "disable_send_url" // IS_SEND_BASE_PAGE_URL_DISABLED
    };

    private ContextualSearchFieldTrial() {
        assert ContextualSearchSwitchNames.length == ContextualSearchSwitch.NUM_ENTRIES;
    }

    /**
     * Current Variations parameters associated with the ContextualSearch Field Trial or a
     * Chrome Feature to determine if the service is enabled
     * (whether Contextual Search is enabled or not).
     */
    public static boolean isEnabled() {
        if (sEnabled == null) sEnabled = detectEnabled();
        return sEnabled.booleanValue();
    }

    static boolean getSwitch(@ContextualSearchSwitch int value) {
        if (sSwitches[value] == null) {
            switch (value) {
                case ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION_ENABLED:
                case ContextualSearchSwitch.IS_CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE_ENABLED:
                    sSwitches[value] =
                            ChromeFeatureList.isEnabled(ContextualSearchSwitchNames[value]);
                    break;
                default:
                    assert !TextUtils.isEmpty(ContextualSearchSwitchNames[value]);
                    sSwitches[value] = getBooleanParam(ContextualSearchSwitchNames[value]);
            }
        }
        return sSwitches[value].booleanValue();
    }

    /**
     * Gets the "stamp" parameter from the RelatedSearches FieldTrial feature.
     * @return The stamp parameter from the feature. If no stamp param is present then an empty
     *         string is returned.
     */
    static String getRelatedSearchesExperimentConfigurationStamp() {
        return getRelatedSearchesParam(RELATED_SEARCHES_CONFIG_STAMP_PARAM_NAME);
    }

    /**
     * Gets the given parameter from the RelatedSearches FieldTrial feature.
     * @param paramName The name of the parameter to get.
     * @return The value of the parameter from the feature. If no param is present then an empty
     *         string is returned.
     */
    static String getRelatedSearchesParam(String paramName) {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.RELATED_SEARCHES, paramName);
    }

    /**
     * Determines whether the specified parameter is present and enabled in the RelatedSearches
     * Feature.
     * @param relatedSearchesParamName The name of the param to get from the Feature.
     * @return Whether the given parameter is enabled or not (has a value of "true").
     */
    static boolean isRelatedSearchesParamEnabled(String relatedSearchesParamName) {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.RELATED_SEARCHES, relatedSearchesParamName, false);
    }

    static boolean showDefaultChipInBar() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.RELATED_SEARCHES_IN_BAR,
                RELATED_SEARCHES_SHOW_DEFAULT_QUERY_CHIP_PARAM_NAME, false);
    }

    static boolean showDefaultChipInPanel() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.RELATED_SEARCHES_ALTERNATE_UX,
                RELATED_SEARCHES_SHOW_DEFAULT_QUERY_CHIP_PARAM_NAME, false);
    }

    /* Return the max width of the bar's default chip in Sp. */
    static int getDefaultChipWidthSpInBar() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.RELATED_SEARCHES_IN_BAR,
                RELATED_SEARCHES_DEFAULT_QUERY_CHIP_MAX_WIDTH_SP_PARAM_NAME, 0);
    }

    /* Return the max width of the panel's default chip in Sp. */
    static int getDefaultChipWidthSpInPanel() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.RELATED_SEARCHES_ALTERNATE_UX,
                RELATED_SEARCHES_DEFAULT_QUERY_CHIP_MAX_WIDTH_SP_PARAM_NAME, 0);
    }

    /* Return the max times to show the promo card. */
    static int getDefaultPromoCardShownTimes() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS,
                CONTEXTUAL_SEARCH_PROMO_CARD_MAX_SHOWN_PARAM_NAME, PROMO_DEFAULT_LIMIT);
    }

    // --------------------------------------------------------------------------------------------
    // Helpers.
    // --------------------------------------------------------------------------------------------

    private static boolean detectEnabled() {
        if (SysUtils.isLowEndDevice()) return false;

        // Allow this user-flippable flag to disable the feature.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_CONTEXTUAL_SEARCH)) {
            return false;
        }

        // Allow this user-flippable flag to enable the feature.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_CONTEXTUAL_SEARCH)) {
            return true;
        }

        // Allow disabling the feature remotely.
        if (getBooleanParam(DISABLED_PARAM)) return false;

        return true;
    }

    /**
     * Gets a boolean Finch parameter, assuming the <paramName>="true" format.  Also checks for
     * a command-line switch with the same name, for easy local testing.
     * @param paramName The name of the Finch parameter (or command-line switch) to get a value
     *                  for.
     * @return Whether the Finch param is defined with a value "true", if there's a command-line
     *         flag present with any value.
     */
    private static boolean getBooleanParam(String paramName) {
        if (CommandLine.getInstance().hasSwitch(paramName)) {
            return true;
        }
        return TextUtils.equals(ENABLED_VALUE,
                VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName));
    }
}
