// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;

import java.util.concurrent.TimeUnit;

/**
 * Commerce Subscriptions Service. TODO(crbug.com/40245507): This service is now only used to manage
 * implicit tracking and to record notification metrics, both of which are Android-specific. The
 * ImplicitPriceDropSubscriptionsManager should be profile-independent and we should decouple
 * subscriptions and notifications. Some logic here like observing Android activity lifecycle can be
 * moved to ShoppingServiceFactory.
 */
public class CommerceSubscriptionsService implements Destroyable {
    @VisibleForTesting
    public static final String CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP =
            ChromePreferenceKeys.COMMERCE_SUBSCRIPTIONS_CHROME_MANAGED_TIMESTAMP;

    private final SharedPreferencesManager mSharedPreferencesManager;
    private final PriceDropNotificationManager mPriceDropNotificationManager;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private PauseResumeWithNativeObserver mPauseResumeWithNativeObserver;
    private ShoppingService mShoppingService;

    /** Creates a new instance. */
    CommerceSubscriptionsService(
            ShoppingService shoppingService,
            PriceDropNotificationManager priceDropNotificationManager) {
        mShoppingService = shoppingService;
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mPriceDropNotificationManager = priceDropNotificationManager;
    }

    /** Performs any deferred startup tasks required by {@link Subscriptions}. */
    public void initDeferredStartupForActivity(
            TabModelSelector tabModelSelector,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mPauseResumeWithNativeObserver =
                new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {
                        maybeRecordMetricsAndInitializeSubscriptions();
                    }

                    @Override
                    public void onPauseWithNative() {}
                };
        mActivityLifecycleDispatcher.register(mPauseResumeWithNativeObserver);
    }

    /**
     * Cleans up internal resources. Currently this method calls SubscriptionsManagerImpl#destroy.
     */
    @Override
    public void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(mPauseResumeWithNativeObserver);
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
        if (!CommerceFeatureUtils.isShoppingListEligible(mShoppingService)) return;
        recordMetricsForEligibleAccount();
    }

    private void recordMetricsForEligibleAccount() {
        // Record notification opt-in metrics.
        mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded();
        mPriceDropNotificationManager.recordMetricsForNotificationCounts();
    }
}
