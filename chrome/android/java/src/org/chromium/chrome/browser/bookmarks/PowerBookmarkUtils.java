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

import java.util.ArrayList;
import java.util.List;

/** Utilities for use in power bookmarks. */
public class PowerBookmarkUtils {
    private static Boolean sPriceTrackingEligibleForTesting;
    private static PowerBookmarkMeta sPowerBookmarkMetaForTesting;

    /**
     * Checks if the given tab is price-trackable.
     * @param tab The tab to check for price-tracking eligibility.
     * @return Whether the given tab is eligible for price-tracking.
     */
    public static boolean isPriceTrackingEligible(@Nullable Tab tab) {
        if (tab == null) return false;
        if (sPriceTrackingEligibleForTesting != null) return sPriceTrackingEligibleForTesting;

        return getPriceTrackingMetadataForTab(tab) != null;
    }

    /**
     * Gets the price tracking metadata for the given tab.
     * @param tab The tab to lookup the metadata for.
     * @return The {@link PowerBookmarkMeta} for the given tab or null.
     */
    public static @Nullable PowerBookmarkMeta getPriceTrackingMetadataForTab(@Nullable Tab tab) {
        if (tab == null) return null;
        if (sPowerBookmarkMetaForTesting != null) return sPowerBookmarkMetaForTesting;

        return ShoppingDataProviderBridge.getForWebContents(tab.getWebContents());
    }

    /**
     * Lookup the cluster id for the given tab and retrieve the corresponding bookmark id which
     * tracks the cluster id.
     * @param tab The tab to lookup the {@link BookmarkId} for.
     * @param bookmarkBridge The {@link BookmarkBridge} used to lookup bookmark data.
     * @return The {@link BookmarkId} for the given tab or null.
     */
    public static List<BookmarkId> getBookmarkIdsWithSharedClusterIdForTab(
            @Nullable Tab tab, BookmarkBridge bookmarkBridge) {
        if (tab == null) return new ArrayList<>();
        PowerBookmarkMeta meta = getPriceTrackingMetadataForTab(tab);
        if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) return new ArrayList<>();
        return getBookmarkIdsForClusterId(
                meta.getShoppingSpecifics().getProductClusterId(), bookmarkBridge);
    }

    /**
     * Checks if the bookmark associated with the given cluster id has price tracking enabled.
     * @param clusterId The cluster id to lookup.
     * @param bookmarkBridge The {@link BookmarkBridge} used to lookup bookmark data.
     * @return The {@link BookmarkId} for the given tab or null.
     */
    public static boolean isPriceTrackingEnabledForClusterId(
            Long clusterId, BookmarkBridge bookmarkBridge) {
        List<BookmarkId> productIds = getBookmarkIdsForClusterId(clusterId, bookmarkBridge);
        for (BookmarkId productId : productIds) {
            PowerBookmarkMeta meta = bookmarkBridge.getPowerBookmarkMeta(productId);

            // Return any of the bookmarks with the given cluster id are price-tracked.
            if (meta != null && meta.getType() == PowerBookmarkType.SHOPPING
                    && meta.getShoppingSpecifics().getIsPriceTracked()) {
                return true;
            }
        }

        return false;
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
    public static void setPriceTrackingEnabled(@Nullable SubscriptionsManager subscriptionsManager,
            @NonNull BookmarkBridge bookmarkBridge, @Nullable BookmarkId bookmarkId,
            boolean enabled, Callback<Integer> callback) {
        if (bookmarkId == null || subscriptionsManager == null) return;

        PowerBookmarkMeta meta = bookmarkBridge.getPowerBookmarkMeta(bookmarkId);
        if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) return;

        CommerceSubscription subscription = createCommerceSubscriptionForPowerBookmarkMeta(meta);
        Callback<Integer> wrapperCallback = (status) -> {
            if (bookmarkBridge.isDestroyed()) return;
            if (status == SubscriptionsManager.StatusCode.OK) {
                setPriceTrackingEnabledInMetadata(bookmarkBridge, bookmarkId, enabled);
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
     * Sets the price-tracking status of the given bookmarks.
     *
     * @param subscriptionsManager Manages price-tracking subscriptions.
     * @param bookmarkBridge The BookmarkBridge used to query bookmarks.
     * @param bookmarkIds A list of BookmarkIds to set the price-tracking status of.
     * @param enabled Whether price-tracking should be enabled.
     * @param snackbarManager Manages snackbars, non-null if a message should be sent to alert the
     *         users of price-tracking events.
     * @param resources Used to retrieve resources.
     */
    public static void setPriceTrackingEnabledWithSnackbars(
            @Nullable SubscriptionsManager subscriptionsManager,
            @NonNull BookmarkBridge bookmarkBridge, @Nullable List<BookmarkId> bookmarkIds,
            boolean enabled, SnackbarManager snackbarManager, Resources resources) {
        if (bookmarkIds == null || bookmarkIds.size() == 0 || subscriptionsManager == null) return;

        // Only the the first bookmark out of the list needs to query subscriptions manager.
        BookmarkId id = bookmarkIds.get(0);
        setPriceTrackingEnabledWithSnackbars(subscriptionsManager, bookmarkBridge, id, enabled,
                snackbarManager, resources, (status) -> {
                    if (status == SubscriptionsManager.StatusCode.OK) {
                        // If the request was successful, set the metadata properly.
                        for (int i = 1; i < bookmarkIds.size(); i++) {
                            setPriceTrackingEnabledInMetadata(
                                    bookmarkBridge, bookmarkIds.get(i), enabled);
                        }
                    }
                });
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
            @Nullable SubscriptionsManager subscriptionsManager,
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
            snackbar.setSingleLine(false);
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

    /** @return Whether the price tracking flag is set in the bookmark's meta. */
    public static boolean isBookmarkPriceTracked(BookmarkModel model, BookmarkId id) {
        PowerBookmarkMeta meta = model.getPowerBookmarkMeta(id);
        if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) return false;

        return meta.getShoppingSpecifics().getIsPriceTracked();
    }

    private static List<BookmarkId> getBookmarkIdsForClusterId(
            Long clusterId, BookmarkBridge bookmarkBridge) {
        List<BookmarkId> results = new ArrayList<>();
        List<BookmarkId> products = bookmarkBridge.getBookmarksOfType(PowerBookmarkType.SHOPPING);
        if (products == null || products.size() == 0) return results;

        for (BookmarkId product : products) {
            PowerBookmarkMeta meta = bookmarkBridge.getPowerBookmarkMeta(product);
            if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) continue;

            Long productClusterId = meta.getShoppingSpecifics().getProductClusterId();
            if (productClusterId.equals(clusterId)) {
                results.add(product);
            }
        }

        return results;
    }

    private static void setPriceTrackingEnabledInMetadata(@NonNull BookmarkBridge bookmarkBridge,
            @Nullable BookmarkId bookmarkId, boolean enabled) {
        PowerBookmarkMeta meta = bookmarkBridge.getPowerBookmarkMeta(bookmarkId);
        if (meta == null || meta.getType() != PowerBookmarkType.SHOPPING) return;

        bookmarkBridge.setPowerBookmarkMeta(bookmarkId,
                PowerBookmarkMeta.newBuilder(meta)
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder(meta.getShoppingSpecifics())
                                        .setIsPriceTracked(enabled)
                                        .build())
                        .build());
    }

    /** Sets the price-tracking eligibility to the test value given. */
    public static void setPriceTrackingEligibleForTesting(@Nullable Boolean enabled) {
        sPriceTrackingEligibleForTesting = enabled;
    }

    /** Sets the current page meta to the test value given. */
    public static void setPowerBookmarkMetaForTesting(@Nullable PowerBookmarkMeta meta) {
        sPowerBookmarkMetaForTesting = meta;
    }
}