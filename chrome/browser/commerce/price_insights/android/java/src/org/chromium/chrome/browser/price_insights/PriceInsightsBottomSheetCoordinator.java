// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the price insights bottom sheet.
 *
 * <p>This component shows bottom sheet content including the price history, price tracking, and
 * jackpot url links of the product.
 */
public class PriceInsightsBottomSheetCoordinator {

    /** Delegate interface for price insights feature. */
    public interface PriceInsightsDelegate {
        /**
         * @param tab Tab whose current URL is checked against.
         * @return BookmarkId or {@link null} if bookmark backend is not loaded.
         */
        @Nullable
        BookmarkId getBookmarkIdForTab(Tab tab);
    }

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModelChangeProcessor<PropertyModel, ? extends View, PropertyKey>
            mChangeProcessor;

    private PriceInsightsBottomSheetContent mBottomSheetContent;
    private PriceInsightsBottomSheetMediator mBottomSheetMediator;
    private View mPriceInsightsView;

    /**
     * @param context The {@link Context} associated with this coordinator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param shoppingService Network service for fetching price insights and price tracking info.
     */
    public PriceInsightsBottomSheetCoordinator(
            @NonNull Context context,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull Tab tab,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ShoppingService shoppingService,
            @NonNull PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        PropertyModel propertyModel =
                new PropertyModel(PriceInsightsBottomSheetProperties.ALL_KEYS);
        mPriceInsightsView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.price_insights_container, /* root= */ null);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        propertyModel,
                        mPriceInsightsView,
                        PriceInsightsBottomSheetViewBinder::bind);
        mBottomSheetMediator =
                new PriceInsightsBottomSheetMediator(
                        mContext,
                        tab,
                        tabModelSelector,
                        shoppingService,
                        priceInsightsDelegate,
                        propertyModel);
    }

    /** Request to show the price insights bottom sheet. */
    public void requestShowContent() {
        mBottomSheetContent = new PriceInsightsBottomSheetContent(mPriceInsightsView);
        mBottomSheetMediator.requestShowContent();
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    /** Close the price insights bottom sheet. */
    public void closeContent() {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
    }
}
