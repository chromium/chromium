// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService;

/** Provides price tracking signal for showing contextual page action for a given tab. */
public class PriceTrackingActionProvider implements ContextualPageActionController.ActionProvider {
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<Profile> mProfileSupplier;

    /** Constructor. */
    public PriceTrackingActionProvider(Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier, Supplier<Profile> profileSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mProfileSupplier = profileSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        final BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            ShoppingService shoppingService = mShoppingServiceSupplier.get();

            // If the user isn't allowed to have the shopping list feature, don't do any more work.
            if (!shoppingService.isShoppingListEligible()) {
                signalAccumulator.setHasPriceTracking(false);
                signalAccumulator.notifySignalAvailable();
                return;
            }

            shoppingService.getProductInfoForUrl(tab.getUrl(), (url, info) -> {
                BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tab);
                boolean canTrackPrice = info != null && info.productClusterId.isPresent();

                if (bookmarkId == null) {
                    signalAccumulator.setHasPriceTracking(canTrackPrice);
                    signalAccumulator.notifySignalAvailable();
                } else {
                    PriceTrackingUtils.isBookmarkPriceTracked(
                            mProfileSupplier.get(), bookmarkId.getId(), (isTracked) -> {
                                // If the product is already tracked, don't make the icon available.
                                signalAccumulator.setHasPriceTracking(canTrackPrice && !isTracked);
                                signalAccumulator.notifySignalAvailable();
                            });
                }
            });
        });
    }
}
