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

/** Provides Field Trial support for the Contextual Search application within Chrome for Android. */
public class ContextualSearchFieldTrial {
    private static final String FIELD_TRIAL_NAME = "ContextualSearch";
    private static final String DISABLED_PARAM = "disabled";
    private static final String ENABLED_VALUE = "true";

    // ==========================================================================================
    // Related Searches FieldTrial and parameter names.
    // ==========================================================================================
    // Params used elsewhere but gathered here since they may be present in FieldTrial configs.
    // A comma-separated list of lower-case ISO 639 language codes.
    static final String RELATED_SEARCHES_LANGUAGE_DEFAULT_ALLOWLIST = "en";
    private static final String RELATED_SEARCHES_CONFIG_DEFAULT_STAMP = "1Rs";
    private static final String RELATED_SEARCHES_ALL_LANGUAGE_CONFIG_DEFAULT_STAMP = "1Rsa";

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
        return ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES_ALL_LANGUAGE)
                ? RELATED_SEARCHES_ALL_LANGUAGE_CONFIG_DEFAULT_STAMP
                : RELATED_SEARCHES_CONFIG_DEFAULT_STAMP;
    }

    /** Return The minimum height dp for the contextual search page. */
    static int getContextualSearchMinimumBasePageHeightDp() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW,
                CONTEXTUAL_SEARCH_MINIMUM_PAGE_HEIGHT_NAME,
                0);
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
        return TextUtils.equals(
                ENABLED_VALUE,
                VariationsAssociatedData.getVariationParamValue(FIELD_TRIAL_NAME, paramName));
    }
}
