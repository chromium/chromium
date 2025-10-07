// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.function.Supplier;

/** Provides price tracking signal for showing contextual page action for a given tab. */
@NullMarked
public class PriceTrackingActionProvider implements ContextualPageActionController.ActionProvider {
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<BookmarkModel> mBookmarkModelSupplier;

    /** Constructor. */
    public PriceTrackingActionProvider(
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {

        if (tab == null || tab.getUrl() == null || !UrlUtilities.isHttpOrHttps(tab.getUrl())) {
            signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.PRICE_TRACKING, false);
            return;
        }

        final BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    ShoppingService shoppingService = mShoppingServiceSupplier.get();

                    // If the user isn't allowed to have the shopping list feature, don't do any
                    // more work.
                    if (!CommerceFeatureUtils.isShoppingListEligible(shoppingService)) {
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.PRICE_TRACKING, false);
                        return;
                    }

                    shoppingService.getProductInfoForUrl(
                            tab.getUrl(),
                            (url, info) -> {
                                boolean canTrackPrice =
                                        info != null && info.productClusterId != null;

                                signalAccumulator.setSignal(
                                        AdaptiveToolbarButtonVariant.PRICE_TRACKING, canTrackPrice);
                            });
                });
    }
}
