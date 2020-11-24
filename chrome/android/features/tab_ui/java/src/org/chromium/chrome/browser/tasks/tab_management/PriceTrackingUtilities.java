// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * A class to handle whether price tracking-related features are turned on by users,
 * including tracking prices on tabs and price drop alerts.
 * Whether the feature is available is controlled by {@link
 * TabUiFeatureUtilities#ENABLE_PRICE_TRACKING}.
 */
public class PriceTrackingUtilities {
    private static final String TRACK_PRICES_ON_TABS =
            ChromePreferenceKeys.PRICE_TRACKING_TRACK_PRICES_ON_TABS;

    private static final SharedPreferencesManager SHARED_PREFERENCES_MANAGER =
            SharedPreferencesManager.getInstance();

    /**
     * Update SharedPreferences when users turn on/off the feature tracking prices on tabs.
     */
    public static void flipTrackPricesOnTabs() {
        final boolean enableTrackPricesOnTabs =
                SHARED_PREFERENCES_MANAGER.readBoolean(TRACK_PRICES_ON_TABS, false);
        SHARED_PREFERENCES_MANAGER.writeBoolean(TRACK_PRICES_ON_TABS, !enableTrackPricesOnTabs);
    }

    /**
     * @return Whether the track prices on tabs is turned on by users.
     */
    public static boolean isTrackPricesOnTabsEnabled() {
        return SHARED_PREFERENCES_MANAGER.readBoolean(TRACK_PRICES_ON_TABS, false);
    }
}
