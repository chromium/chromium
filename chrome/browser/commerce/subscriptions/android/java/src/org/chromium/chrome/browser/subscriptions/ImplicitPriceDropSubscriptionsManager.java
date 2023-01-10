// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * The class that manages Chrome-managed price drop subscriptions.
 */
public class ImplicitPriceDropSubscriptionsManager {
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final ShoppingService mShoppingService;

    public ImplicitPriceDropSubscriptionsManager(
            TabModelSelector tabModelSelector, ShoppingService shoppingService) {
        mShoppingService = shoppingService;
        mTabModelSelector = tabModelSelector;
        mTabModelObserver = new TabModelObserver() {
            @Override
            public void tabClosureCommitted(Tab tab) {
                unsubscribe(tab);
            }

            @Override
            public void tabRemoved(Tab tab) {
                unsubscribe(tab);
            }

            // TODO(crbug.com/1289031): Unsubscribe when user navigates away instead of once
            // selecting the tab.
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                unsubscribe(tab);
            }
        };
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
    }

    private boolean isUniqueTab(Tab tab) {
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        for (int index = 0; index < normalTabModel.getCount(); index++) {
            Tab currentTab = normalTabModel.getTabAt(index);
            if (currentTab.getId() == tab.getId()) {
                continue;
            }
            if (currentTab.getOriginalUrl().getSpec().equals(tab.getOriginalUrl().getSpec())) {
                return false;
            }
        }
        return true;
    }

    /**
     * Initialize the chrome-managed subscriptions.
     */
    void initializeSubscriptions() {
        // Store previously eligible urls to avoid duplicate subscriptions.
        Set<String> urlSet = new HashSet<>();
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        for (int index = 0; index < normalTabModel.getCount(); index++) {
            Tab tab = normalTabModel.getTabAt(index);
            fetchOfferId(tab, (offerId) -> {
                boolean tabEligible = (offerId != null) && isStaleTab(tab);
                RecordHistogram.recordBooleanHistogram(
                        "Commerce.Subscriptions.TabEligible", tabEligible);
                if (!tabEligible) return;
                String url = tab.getOriginalUrl().getSpec();
                if (urlSet.contains(url)) return;
                urlSet.add(url);
                CommerceSubscription subscription = new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK, IdentifierType.OFFER_ID, offerId,
                        ManagementType.CHROME_MANAGED, null);
                mShoppingService.subscribe(subscription, (status) -> {
                    // TODO: Add histograms for implicit tabs creation.
                    assert status;
                });
            });
        }
    }

    private void unsubscribe(Tab tab) {
        if (!isUniqueTab(tab)) return;

        fetchOfferId(tab, (offerId) -> {
            if (offerId == null) return;
            CommerceSubscription subscription =
                    new CommerceSubscription(SubscriptionType.PRICE_TRACK, IdentifierType.OFFER_ID,
                            offerId, ManagementType.CHROME_MANAGED, null);
            mShoppingService.unsubscribe(subscription, (status) -> { assert status; });
        });
    }

    @VisibleForTesting
    protected void fetchOfferId(Tab tab, Callback<String> callback) {
        // Asynchronously fetch the tab's offer id.
        ShoppingPersistedTabData.from(tab, (tabData) -> {
            if (tabData == null || TextUtils.isEmpty(tabData.getMainOfferId())) {
                callback.onResult(null);
            } else {
                callback.onResult(tabData.getMainOfferId());
            }
        });
    }

    // TODO(crbug.com/1186450): Extract this method to a utility class. Also, make the one-day time
    // limit a field parameter.
    private boolean isStaleTab(Tab tab) {
        long timeSinceLastOpened = System.currentTimeMillis()
                - CriticalPersistedTabData.from(tab).getTimestampMillis();

        return timeSinceLastOpened
                <= TimeUnit.SECONDS.toMillis(ShoppingPersistedTabData.getStaleTabThresholdSeconds())
                && timeSinceLastOpened >= TimeUnit.SECONDS.toMillis(
                           CommerceSubscriptionsServiceConfig.getStaleTabLowerBoundSeconds());
    }

    /**
     * Destroy any members that need clean up.
     */
    public void destroy() {
        mTabModelSelector.getModel(false).removeObserver(mTabModelObserver);
    }
}