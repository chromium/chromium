// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Service to expose ShoppingPersistedTabData with price drop information. TODO(crbug.com/40941391):
 * This service should be moved out of current folder when we finish the ShoppingPersistedTabData
 * refactor that will move it out of current folder.
 */
public class ShoppingPersistedTabDataService {
    public static final BooleanCachedFieldTrialParameter
            SKIP_SHOPPING_PERSISTED_TAB_DATA_DELAYED_INITIALIZATION =
                    ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                            ChromeFeatureList.PRICE_CHANGE_MODULE,
                            "skip_shopping_persisted_tab_data_delayed_initialization",
                            true);
    private static ProfileKeyedMap<ShoppingPersistedTabDataService> sProfileToPriceDropService;
    private static ShoppingPersistedTabDataService sServiceForTesting;

    private Set<Tab> mTabsWithPriceDrop;
    private boolean mInitialized;
    private final SharedPreferencesManager mSharedPreferencesManager;

    /**
     * Class for a price change item when externtal components ask for price changes from this
     * service.
     */
    public static class PriceChangeItem {
        private Tab mTab;
        private ShoppingPersistedTabData mData;

        public PriceChangeItem(Tab tab, ShoppingPersistedTabData data) {
            mTab = tab;
            mData = data;
        }

        /**
         * @return the corresponding {@link Tab} of the price drop.
         */
        public Tab getTab() {
            return mTab;
        }

        /**
         * @return the corresponding {@link ShoppingPersistedTabData} of the price drop.
         */
        public ShoppingPersistedTabData getData() {
            return mData;
        }
    }

    /** Creates a new instance. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected ShoppingPersistedTabDataService() {
        mTabsWithPriceDrop = new HashSet<>();
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
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
                profile, (unused) -> new ShoppingPersistedTabDataService());
    }

    /**
     * Initialize the service by passing in the tabs that could have price drop.
     * TODO(crbug.com/40941391): This method could be part of the constructor once
     * ShoppingPersistedTabData is in a separate target.
     *
     * @param tabs the tabs that could have price drop.
     */
    public void initialize(Set<Tab> tabs) {
        if (mInitialized) {
            return;
        }
        mInitialized = true;
        mTabsWithPriceDrop = new HashSet<>(tabs);
    }

    /**
     * Check if the service is initialized.
     *
     * @return whether the service is initialized.
     */
    public boolean isInitialized() {
        return mInitialized;
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
                    // If the service is not initialized at this point, mark the service as
                    // initialized. This usually happens when the service hasn't been initialized
                    // when the deferred initialization of ShoppingPersistedTabData has finished.
                    if (!mInitialized) {
                        mInitialized = true;
                    }
                    if (tab.isDestroyed()) {
                        mTabsWithPriceDrop.remove(tab);
                        mSharedPreferencesManager.removeFromStringSet(
                                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP,
                                String.valueOf(tab.getId()));
                        return;
                    }
                    if (hasDrop) {
                        mTabsWithPriceDrop.add(tab);
                        mSharedPreferencesManager.addToStringSet(
                                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP,
                                String.valueOf(tab.getId()));
                    } else {
                        mTabsWithPriceDrop.remove(tab);
                        mSharedPreferencesManager.removeFromStringSet(
                                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP,
                                String.valueOf(tab.getId()));
                    }
                });
    }

    /**
     * Called by external components to get all the tabs with price drops. The return value is a
     * list of {@link PriceChangeItem} sorted by time that the corresponding Tab was last accessed.
     *
     * @param callback to return the results.
     */
    public void getAllShoppingPersistedTabDataWithPriceDrop(
            Callback<List<PriceChangeItem>> callback) {
        assert mInitialized;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRICE_CHANGE_MODULE)
                || mTabsWithPriceDrop.size() == 0
                || !mInitialized) {
            callback.onResult(new ArrayList<>());
            return;
        }

        Set<Tab> currentTabsWithPriceDrop = new HashSet<>(mTabsWithPriceDrop);
        AtomicInteger counter = new AtomicInteger();
        List<PriceChangeItem> results = new ArrayList<>();

        for (Tab tab : currentTabsWithPriceDrop) {
            ShoppingPersistedTabData.from(
                    tab,
                    result -> {
                        if (isDataEligibleForPriceDrop(result) && !tab.isDestroyed()) {
                            results.add(new PriceChangeItem(tab, result));
                        }
                        // Return when all the data fetching has finished.
                        if (counter.incrementAndGet() == currentTabsWithPriceDrop.size()) {
                            callback.onResult(sortShoppingPersistedTabDataWithPriceDrops(results));
                        }
                    },
                    SKIP_SHOPPING_PERSISTED_TAB_DATA_DELAYED_INITIALIZATION.getValue());
        }
    }

    private static List<PriceChangeItem> sortShoppingPersistedTabDataWithPriceDrops(
            List<PriceChangeItem> data) {
        Collections.sort(
                data,
                (p1, p2) ->
                        Long.compare(
                                p2.getTab().getTimestampMillis(),
                                p1.getTab().getTimestampMillis()));
        return data;
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
    public static void setServiceForTesting(ShoppingPersistedTabDataService service) {
        sServiceForTesting = service;
    }

    /** Get the current set of tabs with price drop for testing. */
    protected Set<Tab> getTabsWithPriceDropForTesting() {
        return mTabsWithPriceDrop;
    }
}
