// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** Provides price tracking signal for showing contextual page action for a given tab. */
@NullMarked
public class PriceTrackingActionProvider implements ContextualPageActionController.ActionProvider {
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<@Nullable BookmarkModel> mBookmarkModelSupplier;

    /** Constructor. */
    public PriceTrackingActionProvider(
            Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<@Nullable BookmarkModel> bookmarkModelSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {

        final GURL tabUrl = tab != null ? tab.getUrl() : null;
        if (tabUrl == null || !UrlUtilities.isHttpOrHttps(tabUrl)) {
            signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.PRICE_TRACKING, false);
            return;
        }

        final BookmarkModel bookmarkModel = assumeNonNull(mBookmarkModelSupplier.get());
        final Supplier<ShoppingService> shoppingServiceSupplier = mShoppingServiceSupplier;
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    ShoppingService shoppingService = shoppingServiceSupplier.get();

                    // If the user isn't allowed to have the shopping list feature, don't do any
                    // more work.
                    if (!CommerceFeatureUtils.isShoppingListEligible(shoppingService)) {
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.PRICE_TRACKING, false);
                        return;
                    }

                    shoppingService.getProductInfoForUrl(
                            tabUrl,
                            (url, info) -> {
                                boolean canTrackPrice =
                                        info != null && info.productClusterId != null;

                                signalAccumulator.setSignal(
                                        AdaptiveToolbarButtonVariant.PRICE_TRACKING, canTrackPrice);
                            });
                });
    }
}
