// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * The class that manages Chrome-managed price drop subscriptions.
 */
public class ImplicitPriceDropSubscriptionsManager {
    @VisibleForTesting
    public static final String CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP =
            ChromePreferenceKeys.COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP;
    @VisibleForTesting
    public static final long CHROME_MANAGED_SUBSCRIPTIONS_TIME_THRESHOLD_MS =
            TimeUnit.SECONDS.toMillis(
                    CommerceSubscriptionsServiceConfig.STALE_TAB_LOWER_BOUND_SECONDS.getValue());
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;
    private final SubscriptionsManagerImpl mSubscriptionManager;
    private final SharedPreferencesManager mSharedPreferencesManager;

    public ImplicitPriceDropSubscriptionsManager(TabModelSelector tabModelSelector,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            SubscriptionsManagerImpl subscriptionsManager) {
        mSubscriptionManager = subscriptionsManager;
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
        };
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
        DeferredStartupHandler.getInstance().addDeferredTask(this::initializeSubscriptions);
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mPauseResumeWithNativeObserver = new PauseResumeWithNativeObserver() {
            @Override
            public void onResumeWithNative() {
                initializeSubscriptions();
            }

            @Override
            public void onPauseWithNative() {}
        };
        mActivityLifecycleDispatcher.register(mPauseResumeWithNativeObserver);
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
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
    @VisibleForTesting
    void initializeSubscriptions() {
        if (!shouldInitializeSubscriptions()) return;
        Map<String, Tab> urlTabMapping = new HashMap<>();
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        for (int index = 0; index < normalTabModel.getCount(); index++) {
            Tab tab = normalTabModel.getTabAt(index);
            if (!hasOfferId(tab) || !isStaleTab(tab)) {
                continue;
            }
            urlTabMapping.put(tab.getOriginalUrl().getSpec(), tab);
        }
        List<CommerceSubscription> subscriptions = new ArrayList<>();
        for (Tab tab : urlTabMapping.values()) {
            CommerceSubscription subscription =
                    new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                            ShoppingPersistedTabData.from(tab).getMainOfferId(),
                            SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
            subscriptions.add(subscription);
        }
        mSubscriptionManager.subscribe(subscriptions);
    }

    private void unsubscribe(Tab tab) {
        if (!isUniqueTab(tab)) {
            return;
        }

        CommerceSubscription subscription =
                new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                        ShoppingPersistedTabData.from(tab).getMainOfferId(),
                        SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
        mSubscriptionManager.unsubscribe(subscription);
    }

    private boolean hasOfferId(Tab tab) {
        return !ShoppingPersistedTabData.from(tab).getMainOfferId().isEmpty();
    }

    // TODO(crbug.com/1186450): Extract this method to a utility class. Also, make the one-day time
    // limit a field parameter.
    private boolean isStaleTab(Tab tab) {
        long tabLastOpenTime = System.currentTimeMillis()
                - CriticalPersistedTabData.from(tab).getTimestampMillis();
        return tabLastOpenTime <= TimeUnit.SECONDS.toMillis(
                       ShoppingPersistedTabData.STALE_TAB_THRESHOLD_SECONDS.getValue())
                && tabLastOpenTime
                >= TimeUnit.SECONDS.toMillis(CommerceSubscriptionsServiceConfig
                                                     .STALE_TAB_LOWER_BOUND_SECONDS.getValue());
    }

    private boolean shouldInitializeSubscriptions() {
        if (System.currentTimeMillis()
                        - mSharedPreferencesManager.readLong(
                                CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP, -1)
                < CHROME_MANAGED_SUBSCRIPTIONS_TIME_THRESHOLD_MS) {
            return false;
        }
        mSharedPreferencesManager.writeLong(
                CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP, System.currentTimeMillis());
        return true;
    }

    /**
     * Destroy any members that need clean up.
     */
    public void destroy() {
        mTabModelSelector.getModel(false).removeObserver(mTabModelObserver);
        mActivityLifecycleDispatcher.unregister(mPauseResumeWithNativeObserver);
    }
}