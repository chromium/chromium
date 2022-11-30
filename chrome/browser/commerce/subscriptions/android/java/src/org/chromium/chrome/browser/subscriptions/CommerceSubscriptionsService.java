// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

import java.util.concurrent.TimeUnit;

/**
 * Commerce Subscriptions Service.
 */
public class CommerceSubscriptionsService {
    @VisibleForTesting
    public static final String CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP =
            ChromePreferenceKeys.COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP;

    private final SubscriptionsManagerImpl mSubscriptionManager;
    private final IdentityManager mIdentityManager;
    private final IdentityManager.Observer mIdentityManagerObserver;
    private final SharedPreferencesManager mSharedPreferencesManager;
    private final PriceDropNotificationManager mPriceDropNotificationManager;
    private final CommerceSubscriptionsMetrics mMetrics;
    private ImplicitPriceDropSubscriptionsManager mImplicitPriceDropSubscriptionsManager;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;

    /** Creates a new instance. */
    CommerceSubscriptionsService(SubscriptionsManagerImpl subscriptionsManager,
            IdentityManager identityManager,
            PriceDropNotificationManager priceDropNotificationManager) {
        mSubscriptionManager = subscriptionsManager;
        mIdentityManager = identityManager;
        mIdentityManagerObserver = new IdentityManager.Observer() {
            @Override
            public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
                mSubscriptionManager.onIdentityChanged();
            }
        };
        mIdentityManager.addObserver(mIdentityManagerObserver);
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mPriceDropNotificationManager = priceDropNotificationManager;
        mMetrics = new CommerceSubscriptionsMetrics();
    }

    /** Performs any deferred startup tasks required by {@link Subscriptions}. */
    public void initDeferredStartupForActivity(TabModelSelector tabModelSelector,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mPauseResumeWithNativeObserver = new PauseResumeWithNativeObserver() {
            @Override
            public void onResumeWithNative() {
                maybeRecordMetricsAndInitializeSubscriptions();
            }

            @Override
            public void onPauseWithNative() {}
        };
        mActivityLifecycleDispatcher.register(mPauseResumeWithNativeObserver);

        if (CommerceSubscriptionsServiceConfig.isImplicitSubscriptionsEnabled()
                && mImplicitPriceDropSubscriptionsManager == null) {
            mImplicitPriceDropSubscriptionsManager = new ImplicitPriceDropSubscriptionsManager(
                    tabModelSelector, mSubscriptionManager);
        }
    }

    /** Returns the subscriptionsManager. */
    public SubscriptionsManagerImpl getSubscriptionsManager() {
        return mSubscriptionManager;
    }

    /**
     * Cleans up internal resources. Currently this method calls SubscriptionsManagerImpl#destroy.
     */
    public void destroy() {
        mIdentityManager.removeObserver(mIdentityManagerObserver);
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(mPauseResumeWithNativeObserver);
        }
        if (mImplicitPriceDropSubscriptionsManager != null) {
            mImplicitPriceDropSubscriptionsManager.destroy();
            mImplicitPriceDropSubscriptionsManager = null;
        }
    }

    private void maybeRecordMetricsAndInitializeSubscriptions() {
        if (System.currentTimeMillis()
                        - mSharedPreferencesManager.readLong(
                                CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP, -1)
                < TimeUnit.SECONDS.toMillis(
                        CommerceSubscriptionsServiceConfig.getStaleTabLowerBoundSeconds())) {
            return;
        }
        mSharedPreferencesManager.writeLong(
                CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP, System.currentTimeMillis());
        mMetrics.recordAccountWaaStatus();
        if (!PriceTrackingFeatures.isPriceDropNotificationEligible()) return;
        recordMetricsForEligibleAccount();
        if (mImplicitPriceDropSubscriptionsManager != null) {
            mImplicitPriceDropSubscriptionsManager.initializeSubscriptions();
        }
    }

    private void recordMetricsForEligibleAccount() {
        // Record notification opt-in metrics.
        mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded();
        mPriceDropNotificationManager.recordMetricsForNotificationCounts();
        mSubscriptionManager.getSubscriptions(
                CommerceSubscriptionType.PRICE_TRACK, false, mMetrics::recordSubscriptionCounts);
    }

    @VisibleForTesting
    void setImplicitSubscriptionsManagerForTesting(ImplicitPriceDropSubscriptionsManager manager) {
        mImplicitPriceDropSubscriptionsManager = manager;
    }
}
