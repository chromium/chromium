// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.variations.VariationsAssociatedData;

import java.util.Map;

/**
 * Provides easy access to data for field trials to do with the Cards UI.
 */
public final class CardsVariationParameters {
    /** Map that stores substitution values for params. */
    private static Map<String, String> sTestVariationParams;

    // Tags are limited to 20 characters.
    private static final String TAG = "CardsVariationParams";

    // Also defined in ntp_snippets_constants.cc
    private static final String FIELD_TRIAL_NAME_VISIBILITY = "NTPSnippetsVisibility";

    private static final String PARAM_FAVICON_SERVICE_NAME = "favicons_fetch_from_service";
    private static final String PARAM_FIRST_CARD_OFFSET = "first_card_offset";
    private static final String PARAM_FIRST_CARD_ANIMATION_MAX_RUNS =
            "first_card_animation_max_runs";
    private static final String PARAM_IGNORE_UPDATES_FOR_EXISTING_SUGGESTIONS =
            "ignore_updates_for_existing_suggestions";

    private static final String PARAM_DISABLED_VALUE = "off";

    private static final int FIRST_CARD_ANIMATION_DEFAULT_VALUE = 7;

    // TODO(sfiera): replace with feature-specific field trials, as has been done in C++.
    private static final String FIELD_TRIAL_NAME_MAIN = "NTPSnippets";

    private CardsVariationParameters() {}

    // TODO(jkrcal): Do a proper general fix in VariationsAssociatedData in the spirit of
    // @Features and ChromeFeatureList.
    /**
     * Sets the parameter values to use in JUnit tests, since native calls are not available there.
     */
    @VisibleForTesting
    public static void setTestVariationParams(Map<String, String> variationParams) {
        sTestVariationParams = variationParams;
    }

    /**
     * Provides the value of the field trial to offset the peeking card (can be overridden
     * with a command line flag). It will return 0 if there is no such field trial.
     */
    public static int getFirstCardOffsetDp() {
        return getIntValue(FIELD_TRIAL_NAME_VISIBILITY, PARAM_FIRST_CARD_OFFSET, 0);
    }

    /**
     * Gets the number of times the first card peeking animation should run.
     */
    public static int getFirstCardAnimationMaxRuns() {
        return getIntValue(FIELD_TRIAL_NAME_VISIBILITY, PARAM_FIRST_CARD_ANIMATION_MAX_RUNS,
                FIRST_CARD_ANIMATION_DEFAULT_VALUE);
    }

    /**
     * @return Whether the NTP should ignore updates for suggestions that have not been seen yet.
     */
    public static boolean ignoreUpdatesForExistingSuggestions() {
        return Boolean.parseBoolean(getParamValue(FIELD_TRIAL_NAME_MAIN,
                PARAM_IGNORE_UPDATES_FOR_EXISTING_SUGGESTIONS));
    }

    public static boolean isFaviconServiceEnabled() {
        return !PARAM_DISABLED_VALUE.equals(getParamValue(FIELD_TRIAL_NAME_MAIN,
                PARAM_FAVICON_SERVICE_NAME));
    }

    private static int getIntValue(String fieldTrialName, String paramName, int defaultValue) {
        // TODO(jkrcal): Get parameter by feature name, not field trial name.
        String value = getParamValue(fieldTrialName, paramName);

        if (!TextUtils.isEmpty(value)) {
            try {
                return Integer.parseInt(value);
            } catch (NumberFormatException ex) {
                Log.w(TAG, "Cannot parse %s experiment value, %s.", paramName, value);
            }
        }

        return defaultValue;
    }

    private static String getParamValue(String fieldTrialName, String paramName) {
        if (sTestVariationParams != null) {
            String value = sTestVariationParams.get(paramName);
            if (value == null) return "";
            return value;
        }

        return VariationsAssociatedData.getVariationParamValue(fieldTrialName, paramName);
    }
}
