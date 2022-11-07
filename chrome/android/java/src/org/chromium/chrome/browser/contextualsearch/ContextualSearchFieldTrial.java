// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.variations.VariationsAssociatedData;

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
    private static final String RELATED_SEARCHES_CONFIG_DEFAULT_STAMP = "1Rs";

    static final String RELATED_SEARCHES_SHOW_DEFAULT_QUERY_CHIP_PARAM_NAME = "default_query_chip";
    static final String RELATED_SEARCHES_DEFAULT_QUERY_CHIP_MAX_WIDTH_SP_PARAM_NAME =
            "default_query_max_width_sp";
    private static final int RELATED_SEARCHES_DEFAULT_QUERY_CHIP_DEFAULT_MAX_WIDTH_SP = 115;

    static final String CONTEXTUAL_SEARCH_MINIMUM_PAGE_HEIGHT_NAME =
            "contextual_search_minimum_page_height_dp";

    // Cached values to avoid repeated and redundant JNI operations.
    private static Boolean sEnabled;

    /**
     * Current Variations parameters associated with the ContextualSearch Field Trial or a
     * Chrome Feature to determine if the service is enabled
     * (whether Contextual Search is enabled or not).
     */
    public static boolean isEnabled() {
        if (sEnabled == null) sEnabled = detectEnabled();
        return sEnabled.booleanValue();
    }

    /**
     * Gets the "stamp" parameter from the RelatedSearches FieldTrial feature.
     * @return The stamp parameter from the feature. If no stamp param is present then an empty
     *         string is returned.
     */
    static String getRelatedSearchesExperimentConfigurationStamp() {
        String stamp = getRelatedSearchesParam(RELATED_SEARCHES_CONFIG_STAMP_PARAM_NAME);
        if (TextUtils.isEmpty(stamp)) {
            stamp = RELATED_SEARCHES_CONFIG_DEFAULT_STAMP;
        }
        return stamp;
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
                RELATED_SEARCHES_SHOW_DEFAULT_QUERY_CHIP_PARAM_NAME, true);
    }

    /* Return the max width of the bar's default chip in Sp. */
    static int getDefaultChipWidthSpInBar() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.RELATED_SEARCHES_IN_BAR,
                RELATED_SEARCHES_DEFAULT_QUERY_CHIP_MAX_WIDTH_SP_PARAM_NAME,
                RELATED_SEARCHES_DEFAULT_QUERY_CHIP_DEFAULT_MAX_WIDTH_SP);
    }

    /** Return The minimum height dp for the contextual search page. */
    static int getContextualSearchMinimumBasePageHeightDp() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW,
                CONTEXTUAL_SEARCH_MINIMUM_PAGE_HEIGHT_NAME, 0);
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
