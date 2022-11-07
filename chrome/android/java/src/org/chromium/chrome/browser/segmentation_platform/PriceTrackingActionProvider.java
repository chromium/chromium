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

    /** Constructor. */
    public PriceTrackingActionProvider(Supplier<ShoppingService> shoppingServiceSupplier,
            Supplier<BookmarkModel> bookmarkModelSupplier) {
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    @Override
    public void getAction(Tab tab, SignalAccumulator signalAccumulator) {
        final BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
        bookmarkModel.finishLoadingBookmarkModel(() -> {
            BookmarkId bookmarkId = bookmarkModel.getUserBookmarkIdForTab(tab);
            boolean isAlreadyPriceTracked = bookmarkId != null
                    && PriceTrackingUtils.isBookmarkPriceTracked(
                            Profile.getLastUsedRegularProfile(), bookmarkId.getId());
            if (isAlreadyPriceTracked) {
                signalAccumulator.setHasPriceTracking(false);
                signalAccumulator.notifySignalAvailable();
            } else {
                mShoppingServiceSupplier.get().getProductInfoForUrl(tab.getUrl(), (url, info) -> {
                    boolean canTrackPrice = info != null;
                    signalAccumulator.setHasPriceTracking(canTrackPrice);
                    signalAccumulator.notifySignalAvailable();
                });
            }
        });
    }
}
