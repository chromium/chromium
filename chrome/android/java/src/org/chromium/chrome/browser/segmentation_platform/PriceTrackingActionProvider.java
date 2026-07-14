// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
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

    private @Nullable CallbackController mCallbackController;

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

        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
        mCallbackController = new CallbackController();

        BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        assert bookmarkModel != null;
        bookmarkModel.finishLoadingBookmarkModel(
                mCallbackController.makeCancelable(() -> runAction(tabUrl, signalAccumulator)));
    }

    private void runAction(GURL tabUrl, SignalAccumulator signalAccumulator) {
        ShoppingService shoppingService = mShoppingServiceSupplier.get();

        // If the user isn't allowed to have the shopping list feature, don't do any more work.
        if (!CommerceFeatureUtils.isShoppingListEligible(shoppingService)) {
            signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.PRICE_TRACKING, false);
            return;
        }

        if (mCallbackController == null) {
            // Should not happen as runAction is called from a callback wrapped by the controller,
            // but check just in case.
            return;
        }

        Callback<ShoppingService.@Nullable ProductInfo> cancelableCallback =
                mCallbackController.makeCancelable(
                        (info) -> {
                            boolean canTrackPrice = info != null && info.productClusterId != null;
                            signalAccumulator.setSignal(
                                    AdaptiveToolbarButtonVariant.PRICE_TRACKING, canTrackPrice);
                        });

        shoppingService.getProductInfoForUrl(
                tabUrl, (url, info) -> cancelableCallback.onResult(info));
    }

    @Override
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }
}
