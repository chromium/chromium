// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
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
         * Get the price tracking state supplier for a {@link Tab}.
         *
         * @param tab Tab whose current URL is checked against.
         * @return The supplier for price tracking state.
         */
        ObservableSupplier<Boolean> getPriceTrackingStateSupplier(Tab tab);

        /**
         * Set price tracking state for a {@link Tab}.
         *
         * @param tab The {@link Tab} to set price tracking state.
         * @param enabled The price tracking state to be set.
         * @param callback The callback when price tracking state is set success or not.
         */
        void setPriceTrackingStateForTab(Tab tab, boolean enabled, Callback<Boolean> callback);

        /**
         * Get the view of the price history chart given the price insights info.
         *
         * @param info The price insights info data.
         * @return The view of the price history chart.
         */
        View getPriceHistoryChartForPriceInsightsInfo(PriceInsightsInfo info);
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
        ScrollView scrollView = (ScrollView) mPriceInsightsView.findViewById(R.id.scroll_view);
        mBottomSheetContent = new PriceInsightsBottomSheetContent(mPriceInsightsView, scrollView);
        mBottomSheetMediator.requestShowContent();
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    /** Close the price insights bottom sheet. */
    public void closeContent() {
        mBottomSheetMediator.closeContent();
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
    }
}
