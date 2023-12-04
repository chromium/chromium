// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.Tab;

import java.util.HashSet;
import java.util.Set;

/**
 * Service to expose ShoppingPersistedTabData with price drop information. TODO(crbug.com/1501138):
 * This service should be moved out of current folder when we finish the ShoppingPersistedTabData
 * refactor that will move it out of current folder.
 */
public class ShoppingPersistedTabDataService {
    private static ProfileKeyedMap<ShoppingPersistedTabDataService> sProfileToPriceDropService;
    private static ShoppingPersistedTabDataService sServiceForTesting;

    private Set<Tab> mTabsWithPriceDrop;

    /** Creates a new instance. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected ShoppingPersistedTabDataService() {
        mTabsWithPriceDrop = new HashSet<>();
    }

    /**
     * Creates a new instance or reuses an existing one based on the current {@link Profile}.
     *
     * @param profile the current {@link Profile}.
     * @return {@link ShoppingPersistedTabDataService} instance for the current regular profile.
     */
    public static ShoppingPersistedTabDataService getForProfile(Profile profile) {
        if (sServiceForTesting != null) {
            return sServiceForTesting;
        }
        if (sProfileToPriceDropService == null) {
            sProfileToPriceDropService =
                    new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileToPriceDropService.getForProfile(
                profile, ShoppingPersistedTabDataService::new);
    }

    /**
     * Called by {@link ShoppingPersistedTabData} to inform the price drop status of given tab.
     *
     * @param tab the {@link Tab} that this notification is about.
     * @param hasDrop whether the tab has price drop or not.
     */
    protected void notifyPriceDropStatus(Tab tab, boolean hasDrop) {
        ThreadUtils.runOnUiThread(
                () -> {
                    if (hasDrop) {
                        mTabsWithPriceDrop.add(tab);
                    } else {
                        mTabsWithPriceDrop.remove(tab);
                    }
                });
    }

    /**
     * Check if a {@link ShoppingPersistedTabData} has all information needed to be rendered in a
     * price change module.
     *
     * @param data the {@link ShoppingPersistedTabData} to check.
     * @return whether the data is eligible.
     */
    protected static boolean isDataEligibleForPriceDrop(@Nullable ShoppingPersistedTabData data) {
        return data != null
                && data.getPriceDrop() != null
                && data.getProductImageUrl() != null
                && data.getProductTitle() != null;
    }

    /** Sets the {@link ShoppingPersistedTabDataService} for testing. */
    protected static void setServiceForTesting(ShoppingPersistedTabDataService service) {
        sServiceForTesting = service;
    }

    /** Get the current set of tabs with price drop for testing. */
    protected Set<Tab> getTabsWithPriceDropForTesting() {
        return mTabsWithPriceDrop;
    }
}
