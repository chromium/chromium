// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.base.DeviceFormFactor;

/** Helper utilities for Vertical Tabs eligibility and preferences. */
@NullMarked
public class VerticalTabUtils {

    /**
     * Returns whether the current device is eligible for Vertical Tabs. Vertical Tabs require the
     * AndroidVerticalTabs feature flag to be enabled and the device to be a tablet form factor.
     */
    public static boolean isVerticalTabsEligible(Context context) {
        return ChromeFeatureList.sAndroidVerticalTabs.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /** Returns whether Vertical Tabs are enabled by both eligibility and user preference. */
    public static boolean isVerticalTabsEnabled(Context context) {
        if (!isVerticalTabsEligible(context)) {
            return false;
        }
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);
    }

    /**
     * Returns whether the "Show Tabs Vertically" entry point action should be displayed. This
     * evaluates to true if the device is eligible for the feature, but the user has not yet
     * switched to the vertical layout view.
     */
    public static boolean shouldShowVerticalTabsEntryPoint(Context context) {
        if (!isVerticalTabsEligible(context)) {
            return false;
        }
        // Return false if the layout is already active (vertical tabs is already visible).
        return !ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);
    }
}
