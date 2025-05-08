// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.Resources;
import android.os.Build;

import com.google.common.primitives.UnsignedLongs;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.CommerceSubscription.UserSeenOffer;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

/** Utilities for use in power bookmarks. */
// TODO(crbug.com/40234642): We should add a JNI layer for the native version of these utilities in
//                price_tracking_utils and use those instead.
@NullMarked
public class PowerBookmarkUtils {
    private static @Nullable Boolean sPriceTrackingEligibleForTesting;
    private static @Nullable PowerBookmarkMeta sPowerBookmarkMetaForTesting;

    /** Returns whether the given meta is a shopping list item. */
    public static boolean isShoppingListItem(
            ShoppingService shoppingService, PowerBookmarkMeta meta) {
        return CommerceFeatureUtils.isShoppingListEligible(shoppingService)
                && meta != null
                && meta.hasShoppingSpecifics();
    }

    /**
     * Checks if the given tab is price-trackable.
     *
     * @param tab The tab to check for price-tracking eligibility.
     * @return Whether the given tab is eligible for price-tracking.
     */
    public static boolean isPriceTrackingEligible(@Nullable Tab tab) {
        if (tab == null || tab.getWebContents() == null) return false;
        if (sPriceTrackingEligibleForTesting != null) return sPriceTrackingEligibleForTesting;

        Profile profile = tab.getProfile();
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        if (service == null) return false;

        ShoppingService.ProductInfo info = service.getAvailableProductInfoForUrl(tab.getUrl());

        return info != null && info.productClusterId.isPresent();
    }

    /**
     * Unified way to get the associated {@link CommerceSubscription} for a {@link
     * PowerBookmarkMeta}.
     *
     * @param meta The {@link PowerBookmarkMeta} to create the {@link CommerceSubscription} for.
     * @return The {@link CommerceSubsription} for the given {@link PowerBookmarkMeta}
     */
    public static CommerceSubscription createCommerceSubscriptionForPowerBookmarkMeta(
            PowerBookmarkMeta meta) {
        return createCommerceSubscriptionForShoppingSpecifics(meta.getShoppingSpecifics());
    }

    /**
     * Unified way to get the associated {@link CommerceSubscription} for a {@link
     * ShoppingSpecifics}.
     *
     * @param shoppingSpecifics The {@link ShoppingSpecifics} to create the {@link
     *     CommerceSubscription} for.
     * @return The {@link CommerceSubsription} for the given {@link ShoppingSpecifics}
     */
    public static CommerceSubscription createCommerceSubscriptionForShoppingSpecifics(
            ShoppingSpecifics shoppingSpecifics) {
        // Use UnsignedLongs to convert ProductClusterId to avoid overflow.
        UserSeenOffer seenOffer =
                new UserSeenOffer(
                        UnsignedLongs.toString(shoppingSpecifics.getOfferId()),
                        shoppingSpecifics.getCurrentPrice().getAmountMicros(),
                        shoppingSpecifics.getCountryCode(),
                        shoppingSpecifics.getLocale());
        return new CommerceSubscription(
                SubscriptionType.PRICE_TRACK,
                IdentifierType.PRODUCT_CLUSTER_ID,
                UnsignedLongs.toString(shoppingSpecifics.getProductClusterId()),
                ManagementType.USER_MANAGED,
                seenOffer);
    }

    /**
     * Checks if the given {@link BookmarkId} is price-tracked.
     *
     * @param bookmarkModel The BookmarkModel used to query bookmarks.
     * @param bookmarkId The BookmarkId to check the price-tracking status of.
     * @param enabled Whether price-tracking should be enabled.
     * @param snackbarManager Manages snackbars, non-null if a message should be sent to alert the
     *     users of price-tracking events.
     * @param resources Used to retrieve resources.
     * @param profile The current profile.
     * @param callback The status callback, may be called multiple times depending if the user
     *     retries on failure.
     * @param priceDropNotificationManager Manages price drop notifications.
     */
    public static void setPriceTrackingEnabledWithSnackbars(
            BookmarkModel bookmarkModel,
            @Nullable BookmarkId bookmarkId,
            boolean enabled,
            SnackbarManager snackbarManager,
            Resources resources,
            Profile profile,
            Callback<Boolean> callback,
            PriceDropNotificationManager priceDropNotificationManager) {
        // TODO(crbug.com/393186352): Fix nullable annotation for bookmarkId parameter.
        // Early return when bookmarkId is null.
        if (bookmarkId == null) {
            Snackbar snackbar =
                    Snackbar.make(
                            resources.getString(R.string.price_tracking_error_snackbar),
                            null,
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_PRICE_TRACKING_FAILURE);
            snackbar.setSingleLine(false);
            snackbarManager.showSnackbar(snackbar);
            callback.onResult(false);
            return;
        }

        // Action to retry the subscription request on failure.
        SnackbarManager.SnackbarController retrySnackbarControllerAction =
                new SnackbarManager.SnackbarController() {
                    @Override
                    public void onAction(@Nullable Object actionData) {
                        setPriceTrackingEnabledWithSnackbars(
                                bookmarkModel,
                                bookmarkId,
                                enabled,
                                snackbarManager,
                                resources,
                                profile,
                                callback,
                                priceDropNotificationManager);
                    }
                };
        // Wrapper which shows a snackbar and forwards the result.
        Callback<Boolean> wrapperCallback =
                (success) -> {
                    Snackbar snackbar;
                    if (success) {
                        snackbar =
                                Snackbar.make(
                                        resources.getString(
                                                enabled
                                                        ? R.string.price_tracking_enabled_snackbar
                                                        : R.string
                                                                .price_tracking_disabled_snackbar),
                                        null,
                                        Snackbar.TYPE_NOTIFICATION,
                                        Snackbar.UMA_PRICE_TRACKING_SUCCESS);
                    } else {
                        snackbar =
                                Snackbar.make(
                                                resources.getString(
                                                        R.string.price_tracking_error_snackbar),
                                                retrySnackbarControllerAction,
                                                Snackbar.TYPE_NOTIFICATION,
                                                Snackbar.UMA_PRICE_TRACKING_FAILURE)
                                        .setAction(
                                                resources.getString(
                                                        R.string
                                                                .price_tracking_error_snackbar_action),
                                                null);
                    }
                    snackbar.setSingleLine(false);
                    snackbarManager.showSnackbar(snackbar);
                    callback.onResult(success);
                };
        // Make sure the notification channel is initialized when the user tracks a product.
        // TODO(crbug.com/40245507): Add a SubscriptionsObserver in the PriceDropNotificationManager
        // and initialize the channel there.
        if (enabled && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            priceDropNotificationManager.createNotificationChannel();
        }
        PriceTrackingUtils.setPriceTrackingStateForBookmark(
                profile, bookmarkId.getId(), enabled, wrapperCallback);
    }

    /**
     * Gets the power bookmark associated with the given tab.
     *
     * @param bookmarkModel The {@link BookmarkModel} to retrieve bookmark info.
     * @param tab The current {@link Tab} to check.
     * @return The {@link PowerBookmarkMeta} associated with the given tab or null.
     */
    public static @Nullable PowerBookmarkMeta getBookmarkBookmarkMetaForTab(
            @Nullable BookmarkModel bookmarkModel, @Nullable Tab tab) {
        if (bookmarkModel == null || tab == null) return null;

        BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tab);
        if (bookmarkId == null) return null;

        return bookmarkModel.getPowerBookmarkMeta(bookmarkId);
    }

    /** Sets the price-tracking eligibility to the test value given. */
    public static void setPriceTrackingEligibleForTesting(@Nullable Boolean enabled) {
        sPriceTrackingEligibleForTesting = enabled;
        ResettersForTesting.register(() -> sPriceTrackingEligibleForTesting = null);
    }

    /** Sets the current page meta to the test value given. */
    public static void setPowerBookmarkMetaForTesting(@Nullable PowerBookmarkMeta meta) {
        sPowerBookmarkMetaForTesting = meta;
        ResettersForTesting.register(() -> sPowerBookmarkMetaForTesting = null);
    }
}
