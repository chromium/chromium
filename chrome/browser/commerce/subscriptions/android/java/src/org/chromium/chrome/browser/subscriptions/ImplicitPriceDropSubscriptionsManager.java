// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
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

    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;
    private final SubscriptionsManagerImpl mSubscriptionManager;
    private final SharedPreferencesManager mSharedPreferencesManager;
    private final PriceDropNotificationManager mPriceDropNotificationManager;

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
        mPriceDropNotificationManager = new PriceDropNotificationManager();
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
            if (!hasOfferId(tab)) {
                continue;
            }

            CommerceSubscription subscription =
                    new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                            ShoppingPersistedTabData.from(tab).getMainOfferId(),
                            SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
            subscriptions.add(subscription);
        }

        mSubscriptionManager.subscribe(subscriptions, (status) -> {
            // TODO: Add histograms for implicit tabs creation.
            assert status == SubscriptionsManager.StatusCode.OK;
        });
    }

    private void unsubscribe(Tab tab) {
        if (!isUniqueTab(tab) || !hasOfferId(tab)) {
            return;
        }

        CommerceSubscription subscription =
                new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                        ShoppingPersistedTabData.from(tab).getMainOfferId(),
                        SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
        mSubscriptionManager.unsubscribe(
                subscription, (status) -> { assert status == SubscriptionsManager.StatusCode.OK; });
    }

    private boolean hasOfferId(Tab tab) {
        return ShoppingPersistedTabData.from(tab) != null
                && !TextUtils.isEmpty(ShoppingPersistedTabData.from(tab).getMainOfferId());
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

    private boolean shouldInitializeSubscriptions() {
        if ((!mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded())
                || (System.currentTimeMillis()
                                - mSharedPreferencesManager.readLong(
                                        CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP, -1)
                        < TimeUnit.SECONDS.toMillis(CommerceSubscriptionsServiceConfig
                                                            .getStaleTabLowerBoundSeconds()))) {
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