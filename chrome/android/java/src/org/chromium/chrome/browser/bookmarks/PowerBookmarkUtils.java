// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.Resources;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.common.primitives.UnsignedLongs;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.shopping_list.ShoppingDataProviderBridge;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkType;
import org.chromium.chrome.browser.power_bookmarks.ProductPrice;
import org.chromium.chrome.browser.power_bookmarks.ShoppingSpecifics;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;

/** Utilities for use in power bookmarks. */
public class PowerBookmarkUtils {
    private static Boolean sPriceTrackingEligibleForTesting;

    /**
     * Checks if the given tab is price-trackable.
     * @param tab The tab to check for price-tracking eligibility.
     * @return Whether the given tab is eligible for price-tracking.
     */
    public static boolean isPriceTrackingEligible(@Nullable Tab tab) {
        if (tab == null) return false;
        if (sPriceTrackingEligibleForTesting != null) return sPriceTrackingEligibleForTesting;

        return ShoppingDataProviderBridge.getForWebContents(tab.getWebContents()) != null;
    }

    /**
     * Unified way to get the associated {@link CommerceSubscription} for a {@link
     * PowerBookmarkMeta}.
     * @param meta The {@link PowerBookmarkMeta} to create the {@link CommerceSubscription} for.
     * @return The {@link CommerceSubsription} for the given {@link PowerBookmarkMeta}
     */
    public static @NonNull CommerceSubscription createCommerceSubscriptionForPowerBookmarkMeta(
            @NonNull PowerBookmarkMeta meta) {
        // Use UnsignedLongs to convert ProductClusterId to avoid overflow.
        return new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                UnsignedLongs.toString(meta.getShoppingSpecifics().getProductClusterId()),
                SubscriptionManagementType.USER_MANAGED, TrackingIdType.PRODUCT_CLUSTER_ID);
    }

    /**
     * Checks if the given {@link BookmarkId} is price-tracked.
     *
     * @param subscriptionsManager Manages price-tracking subscriptions.
     * @param snackbarManager Manages snackbars, non-null if a message should be sent to alert the
     *         users of price-tracking events.
     * @param bookmarkBridge The BookmarkBridge used to query bookmarks.
     * @param bookmarkId The BookmarkId to check the price-tracking status of.
     * @param enabled Whether price-tracking should be enabled.
     * @param callback The status callback.
     */
    public static void setPriceTrackingEnabled(@NonNull SubscriptionsManager subscriptionsManager,
            @NonNull BookmarkBridge bookmarkBridge, @Nullable BookmarkId bookmarkId,
            boolean enabled, Callback<Integer> callback) {
        if (bookmarkId == null) return;

        PowerBookmarkMeta meta = bookmarkBridge.getPowerBookmarkMeta(bookmarkId);
        if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) return;

        CommerceSubscription subscription = createCommerceSubscriptionForPowerBookmarkMeta(meta);
        Callback<Integer> wrapperCallback = (status) -> {
            if (bookmarkBridge.isDestroyed()) return;
            if (status == SubscriptionsManager.StatusCode.OK) {
                bookmarkBridge.setPowerBookmarkMeta(bookmarkId,
                        PowerBookmarkMeta.newBuilder(meta)
                                .setShoppingSpecifics(
                                        ShoppingSpecifics.newBuilder(meta.getShoppingSpecifics())
                                                .setIsPriceTracked(enabled)
                                                .build())
                                .build());
            }
            callback.onResult(status);
        };

        if (enabled) {
            subscriptionsManager.subscribe(subscription, wrapperCallback);
        } else {
            subscriptionsManager.unsubscribe(subscription, wrapperCallback);
        }
    }

    /**
     * Checks if the given {@link BookmarkId} is price-tracked.
     *
     * @param subscriptionsManager Manages price-tracking subscriptions.
     * @param bookmarkBridge The BookmarkBridge used to query bookmarks.
     * @param bookmarkId The BookmarkId to check the price-tracking status of.
     * @param enabled Whether price-tracking should be enabled.
     * @param snackbarManager Manages snackbars, non-null if a message should be sent to alert the
     *         users of price-tracking events.
     * @param resources Used to retrieve resources.
     * @param callback The status callback, may be called multiple times depending if the user
     *         retries on failure.
     */
    public static void setPriceTrackingEnabledWithSnackbars(
            @NonNull SubscriptionsManager subscriptionsManager,
            @NonNull BookmarkBridge bookmarkBridge, @Nullable BookmarkId bookmarkId,
            boolean enabled, SnackbarManager snackbarManager, Resources resources,
            Callback<Integer> callback) {
        // Action to retry the subscription request on failure.
        SnackbarManager.SnackbarController retrySnackbarControllerAction =
                new SnackbarManager.SnackbarController() {
                    @Override
                    public void onAction(Object actionData) {
                        setPriceTrackingEnabledWithSnackbars(subscriptionsManager, bookmarkBridge,
                                bookmarkId, enabled, snackbarManager, resources, callback);
                    }
                };
        // Wrapper which shows a snackbar and forwards the result.
        Callback<Integer> wrapperCallback = (status) -> {
            Snackbar snackbar;
            if (status == SubscriptionsManager.StatusCode.OK) {
                snackbar = Snackbar.make(
                        resources.getString(enabled ? R.string.price_tracking_enabled_snackbar
                                                    : R.string.price_tracking_disabled_snackbar),
                        null, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_PRICE_TRACKING_SUCCESS);
            } else {
                snackbar =
                        Snackbar.make(resources.getString(R.string.price_tracking_error_snackbar),
                                        retrySnackbarControllerAction, Snackbar.TYPE_NOTIFICATION,
                                        Snackbar.UMA_PRICE_TRACKING_FAILURE)
                                .setAction(resources.getString(
                                                   R.string.price_tracking_error_snackbar_action),
                                        null);
            }
            snackbarManager.showSnackbar(snackbar);
            callback.onResult(status);
        };
        setPriceTrackingEnabled(
                subscriptionsManager, bookmarkBridge, bookmarkId, enabled, wrapperCallback);
    }

    /**
     * Update to the given price for the given bookmark id.
     *
     * @param bookmarkBridge Used to read/write bookmark data.
     * @param bookmarkId The bookmark id to update.
     * @param price The price to update to.
     */
    public static void updatePriceForBookmarkId(@NonNull BookmarkBridge bookmarkBridge,
            @NonNull BookmarkId bookmarkId,
            @NonNull org.chromium.components.commerce.PriceTracking.ProductPrice price) {
        PowerBookmarkMeta meta = bookmarkBridge.getPowerBookmarkMeta(bookmarkId);
        if (meta == null) return;
        ProductPrice newPrice = ProductPrice.newBuilder()
                                        .setCurrencyCode(price.getCurrencyCode())
                                        .setAmountMicros(price.getAmountMicros())
                                        .build();
        bookmarkBridge.setPowerBookmarkMeta(bookmarkId,
                PowerBookmarkMeta.newBuilder(meta)
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder(meta.getShoppingSpecifics())
                                        .setCurrentPrice(newPrice)
                                        .build())
                        .build());
    }

    /**
     * Gets the power bookmark associated with the given tab.
     * @param bookmarkBridge The {@link BookmarkBridge} to retrieve bookmark info.
     * @param tab The current {@link Tab} to check.
     * @return The {@link PowerBookmarkMeta} associated with the given tab or null.
     */
    public static @Nullable PowerBookmarkMeta getBookmarkBookmarkMetaForTab(
            @Nullable BookmarkBridge bookmarkBridge, @Nullable Tab tab) {
        if (bookmarkBridge == null || tab == null) return null;

        BookmarkId bookmarkId = bookmarkBridge.getUserBookmarkIdForTab(tab);
        if (bookmarkId == null) return null;

        return bookmarkBridge.getPowerBookmarkMeta(bookmarkId);
    }

    /** Sets the price-tracking eligibility to the test value given. */
    public static void setPriceTrackingEligibleForTesting(@Nullable Boolean enabled) {
        sPriceTrackingEligibleForTesting = enabled;
    }

    /** @return Whether the price tracking flag is set in the bookmark's meta. */
    public static boolean isBookmarkPriceTracked(BookmarkModel model, BookmarkId id) {
        PowerBookmarkMeta meta = model.getPowerBookmarkMeta(id);
        if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) return false;

        return meta.getShoppingSpecifics().getIsPriceTracked();
    }
}