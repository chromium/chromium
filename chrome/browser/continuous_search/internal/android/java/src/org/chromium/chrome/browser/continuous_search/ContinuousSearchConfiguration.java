// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Contains configuration and permanent state for the Continuous Search feature.
 */
class ContinuousSearchConfiguration {
    @VisibleForTesting
    static final String CONTINUOUS_SEARCH_DISMISSAL_COUNT =
            ChromePreferenceKeys.CONTINUOUS_SEARCH_DISMISSAL_COUNT;

    @VisibleForTesting
    static final SharedPreferencesManager SHARED_PREFERENCES_MANAGER =
            SharedPreferencesManager.getInstance();

    static final String PERMANENT_DISMISSAL_THRESHOLD = "permanent_dismissal_threshold";
    static final int IGNORE_DISMISSALS = -1;

    /**
     * Initializes the configuration by resetting dismissal state if it is stale.
     */
    static void initialize() {
        if (ignoreDismissals()) {
            SHARED_PREFERENCES_MANAGER.writeInt(CONTINUOUS_SEARCH_DISMISSAL_COUNT, 0);
            return;
        }
    }

    /**
     * Records that the UI was dismissed.
     */
    static void recordDismissed() {
        if (ignoreDismissals() || isPermanentlyDismissed()) return;

        SHARED_PREFERENCES_MANAGER.incrementInt(CONTINUOUS_SEARCH_DISMISSAL_COUNT);
    }

    /**
     * Returns whether the feature is considered to be permanently dismissed.
     */
    static boolean isPermanentlyDismissed() {
        if (ignoreDismissals()) return false;

        int dismissals = SHARED_PREFERENCES_MANAGER.readInt(CONTINUOUS_SEARCH_DISMISSAL_COUNT);
        return dismissals >= getPermanentDismissalThreshold();
    }

    /**
     * Gets the threshold required to permanently dismiss the feature.
     */
    private static int getPermanentDismissalThreshold() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTINUOUS_SEARCH, PERMANENT_DISMISSAL_THRESHOLD,
                IGNORE_DISMISSALS);
    }

    /**
     * Whether to ignore dismissal counts.
     */
    private static boolean ignoreDismissals() {
        return getPermanentDismissalThreshold() == IGNORE_DISMISSALS;
    }
}
