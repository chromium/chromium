// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
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
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;

import java.util.HashSet;
import java.util.Set;
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

            // TODO(crbug.com/1289031): Unsubscribe when user navigates away instead of once
            // selecting the tab.
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
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
        // TODO(crbug.com/1271234): Cleanup the usage of PriceDropNotificationManager. When user
        // turns off price notification, we still subscribe for tabs. Now we call this method only
        // to record notification opt-in metrics.
        mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded();
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
                CommerceSubscription subscription =
                        new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK, offerId,
                                SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
                mSubscriptionManager.subscribe(subscription, (status) -> {
                    // TODO: Add histograms for implicit tabs creation.
                    assert status == SubscriptionsManager.StatusCode.OK;
                });
            });
        }
    }

    private void unsubscribe(Tab tab) {
        if (!isUniqueTab(tab)) return;

        fetchOfferId(tab, (offerId) -> {
            if (offerId == null) return;
            CommerceSubscription subscription =
                    new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK, offerId,
                            SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
            mSubscriptionManager.unsubscribe(subscription,
                    (status) -> { assert status == SubscriptionsManager.StatusCode.OK; });
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

    private boolean shouldInitializeSubscriptions() {
        if ((!PriceTrackingUtilities.isPriceDropNotificationEligible())
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