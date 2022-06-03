// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Commerce Subscriptions Service.
 */
public class CommerceSubscriptionsService {
    private final SubscriptionsManagerImpl mSubscriptionManager;
    private ImplicitPriceDropSubscriptionsManager mImplicitPriceDropSubscriptionsManager;

    /** Creates a new instance. */
    CommerceSubscriptionsService(SubscriptionsManagerImpl subscriptionsManager) {
        mSubscriptionManager = subscriptionsManager;
    }

    /** Performs any deferred startup tasks required by {@link Subscriptions}. */
    public void initDeferredStartupForActivity(TabModelSelector tabModelSelector,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {
        if (mImplicitPriceDropSubscriptionsManager == null) {
            mImplicitPriceDropSubscriptionsManager = new ImplicitPriceDropSubscriptionsManager(
                    tabModelSelector, activityLifecycleDispatcher, mSubscriptionManager);
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
        if (mImplicitPriceDropSubscriptionsManager != null) {
            mImplicitPriceDropSubscriptionsManager.destroy();
        }
    }
}
