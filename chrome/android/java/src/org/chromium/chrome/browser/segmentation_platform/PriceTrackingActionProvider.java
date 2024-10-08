// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlUtilities;

/** Provides price tracking signal for showing contextual page action for a given tab. */
public class PriceTrackingActionProvider implements ContextualPageActionController.ActionProvider {
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<Profile> mProfileSupplier;

    /** Constructor. */
    public PriceTrackingActionProvider(
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier,
            Supplier<Profile> profileSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mProfileSupplier = profileSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {

        if (tab == null || tab.getUrl() == null || !UrlUtilities.isHttpOrHttps(tab.getUrl())) {
            signalAccumulator.setHasPriceTracking(false);
            signalAccumulator.notifySignalAvailable();
            return;
        }

        final BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    ShoppingService shoppingService = mShoppingServiceSupplier.get();

                    // If the user isn't allowed to have the shopping list feature, don't do any
                    // more work.
                    if (!CommerceFeatureUtils.isShoppingListEligible(shoppingService)) {
                        signalAccumulator.setHasPriceTracking(false);
                        signalAccumulator.notifySignalAvailable();
                        return;
                    }

                    shoppingService.getProductInfoForUrl(
                            tab.getUrl(),
                            (url, info) -> {
                                boolean canTrackPrice =
                                        info != null && info.productClusterId.isPresent();

                                signalAccumulator.setHasPriceTracking(canTrackPrice);
                                signalAccumulator.notifySignalAvailable();
                            });
                });
    }
}
