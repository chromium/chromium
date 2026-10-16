// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.ACTIVE_PAGE_INDEX;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.PAGE_COUNT;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.SUBTITLE_RES_ID;
import static org.chromium.chrome.browser.safety_promo.SafetyPromoCarouselProperties.TITLE_RES_ID;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator for the Safety Promo Carousel. Decides which page is active and writes its state to the
 * {@link PropertyModel}; it never touches views.
 *
 * <p>The active page follows the item picked on the overview page, and the page the user swipes to,
 * as reported via {@link #onPageSnapped(int)}.
 */
@NullMarked
class SafetyPromoCarouselMediator {
    private final PropertyModel mModel;
    private final NullableObservableSupplier<SafetyPromoItem> mSelectedItemSupplier;
    private final Callback<@Nullable SafetyPromoItem> mSelectedItemObserver;
    private final List<SafetyPromoItem> mItems;

    /**
     * @param model The {@link PropertyModel} of the carousel.
     * @param selectedItemSupplier Supplier of the {@link SafetyPromoItem} picked on the overview
     *     page.
     * @param items The list of {@link SafetyPromoItem}s displayed in the carousel.
     */
    SafetyPromoCarouselMediator(
            PropertyModel model,
            NullableObservableSupplier<SafetyPromoItem> selectedItemSupplier,
            List<SafetyPromoItem> items) {
        // The carousel promo should only be displayed when promo items are configured.
        assert !items.isEmpty();

        mModel = model;
        mSelectedItemSupplier = selectedItemSupplier;
        mSelectedItemObserver = this::onSelectedItemChanged;
        mItems = items;

        mModel.set(PAGE_COUNT, mItems.size());
        setActivePage(getPositionForItem(selectedItemSupplier.get()));
        selectedItemSupplier.addSyncObserver(mSelectedItemObserver);
    }

    /** Stops observing the selected item. */
    void destroy() {
        mSelectedItemSupplier.removeObserver(mSelectedItemObserver);
    }

    /**
     * Called when the carousel has settled on a page after the user scrolled it.
     *
     * @param position The index of the page the carousel snapped to.
     */
    void onPageSnapped(int position) {
        setActivePage(position);
    }

    private void onSelectedItemChanged(@Nullable SafetyPromoItem item) {
        if (item == null) return;

        setActivePage(getPositionForItem(item));
    }

    private int getPositionForItem(@Nullable SafetyPromoItem item) {
        if (item == null) return 0;

        int position = mItems.indexOf(item);
        return Math.max(0, position);
    }

    /**
     * Updates the property model with the active page's title, subtitle, and index.
     *
     * @param position The index of the carousel item to make active.
     */
    private void setActivePage(int position) {
        assert position >= 0 && position < mItems.size();

        SafetyPromoItem item = mItems.get(position);
        mModel.set(TITLE_RES_ID, item.carouselTitleResId);
        mModel.set(SUBTITLE_RES_ID, item.carouselSubtitleResId);
        mModel.set(ACTIVE_PAGE_INDEX, position);
    }
}
