// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import com.google.common.primitives.UnsignedLongs;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.url.GURL;

/**
 * This class provides the price tracking state for the current page, it keeps track of the current
 * page by listening to navigations and tab changes, and it listens to ShoppingService for updates
 * within the same page.
 */
@NullMarked
public class CurrentTabPriceTrackingStateSupplier implements NonNullObservableSupplier<Boolean> {

    private CurrentTabObserver mCurrentTabObserver;
    private @Nullable CommerceSubscription mCurrentTabCommerceSubscription;
    private @Nullable ShoppingService mShoppingService;

    private final SettableNonNullObservableSupplier<Boolean> mSupplier =
            ObservableSuppliers.createNonNull(false);
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mOnProfileUpdatedCallback = this::onProfileUpdated;
    private final SubscriptionsObserver mSubscriptionObserver =
            new SubscriptionsObserver() {
                @Override
                public void onSubscribe(CommerceSubscription subscription, boolean succeeded) {
                    if (succeeded && subscription.equals(mCurrentTabCommerceSubscription)) {
                        updatePriceTrackingState(true);
                    }
                }

                @Override
                public void onUnsubscribe(CommerceSubscription subscription, boolean succeeded) {
                    if (succeeded && subscription.equals(mCurrentTabCommerceSubscription)) {
                        updatePriceTrackingState(false);
                    }
                }
            };

    /**
     * Creates an instance of CurrentTabPriceTrackingStateSupplier and starts observing the current
     * tab and its price tracking state.
     *
     * @param tabSupplier Supplier for the currently active tab.
     * @param profileSupplier Profile supplier, used to retrieve a ShoppingService from it.
     */
    public CurrentTabPriceTrackingStateSupplier(
            NullableObservableSupplier<Tab> tabSupplier,
            MonotonicObservableSupplier<Profile> profileSupplier) {
        mTabSupplier = tabSupplier;
        mProfileSupplier = profileSupplier;

        startObserving();
    }

    private void startObserving() {
        // Create a CurrentTabObserver to keep track of the currently visible page, it tracks page
        // navigations and tab changes.
        mCurrentTabObserver =
                new CurrentTabObserver(
                        mTabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onUrlUpdated(Tab tab) {
                                refreshPriceTrackingState();
                            }
                        },
                        /* swapCallback= */ tab -> refreshPriceTrackingState());

        // Check for profile availability so we can create a ShoppingService which we'll use to keep
        // track of subscription changes in the current page.
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mOnProfileUpdatedCallback);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        mCurrentTabObserver.destroy();
        mCurrentTabObserver = null;

        mProfileSupplier.removeObserver(mOnProfileUpdatedCallback);

        if (mShoppingService != null) {
            mShoppingService.removeSubscriptionsObserver(mSubscriptionObserver);
            mShoppingService = null;
        }
        mSupplier.destroy();
    }

    private void onProfileUpdated(Profile profile) {
        if (mShoppingService != null) {
            mShoppingService.removeSubscriptionsObserver(mSubscriptionObserver);
        }
        mShoppingService = ShoppingServiceFactory.getForProfile(profile);
        mShoppingService.addSubscriptionsObserver(mSubscriptionObserver);
    }

    private void refreshPriceTrackingState() {
        if (mTabSupplier.get() == null
                || mTabSupplier.get().getUrl() == null
                || mShoppingService == null) {
            return;
        }

        mShoppingService.getProductInfoForUrl(
                mTabSupplier.get().getUrl(), this::onProductInfoRetrieved);
    }

    private void onProductInfoRetrieved(GURL checkedUrl, @Nullable ProductInfo productInfo) {
        if (productInfo == null
                || productInfo.productClusterId == null
                || mShoppingService == null) {
            mCurrentTabCommerceSubscription = null;
            updatePriceTrackingState(false);
            return;
        }

        // Store this subscription object to listen to changes to the price tracking state in
        // mSubscriptionObserver.
        mCurrentTabCommerceSubscription =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        UnsignedLongs.toString(productInfo.productClusterId),
                        ManagementType.USER_MANAGED,
                        null);

        mShoppingService.isSubscribed(
                mCurrentTabCommerceSubscription,
                isCurrentTabPriceTracked -> {
                    // Get URL for current tab again, as the tab may have changed while loading this
                    // result.
                    var tab = mTabSupplier.get();
                    if (tab == null) return;

                    var currentUrl = tab.getUrl();
                    // Ensure we're still in the same tab.
                    if (!checkedUrl.equals(currentUrl)) {
                        return;
                    }

                    updatePriceTrackingState(isCurrentTabPriceTracked);
                });
    }

    private void updatePriceTrackingState(boolean isCurrentTabPriceTracked) {
        mSupplier.set(isCurrentTabPriceTracked);
    }

    @Override
    public Boolean get() {
        return mSupplier.get();
    }

    @Override
    public Boolean addObserver(Callback<Boolean> obs, int behavior) {
        return mSupplier.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<Boolean> obs) {
        mSupplier.removeObserver(obs);
    }

    @Override
    public int getObserverCount() {
        return mSupplier.getObserverCount();
    }
}
