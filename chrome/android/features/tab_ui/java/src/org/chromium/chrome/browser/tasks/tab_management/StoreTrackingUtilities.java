// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;

/**
 * A class to handle whether store hours feature is enabled.
 */
public class StoreTrackingUtilities {
    /**
     * @return Whether the show store hours on tabs feature is enabled.
     */
    public static boolean isStoreHoursOnTabsEnabled() {
        return ChromeFeatureList.sStoreHoursAndroid.isEnabled()
                && !PriceTrackingUtilities.isTrackPricesOnTabsEnabled();
    }
}
