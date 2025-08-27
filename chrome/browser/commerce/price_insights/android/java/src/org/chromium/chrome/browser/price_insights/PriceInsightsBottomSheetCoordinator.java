// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the price insights bottom sheet.
 *
 * <p>This component shows bottom sheet content including the price history, price tracking, and
 * jackpot url links of the product.
 */
@NullMarked
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
        @Nullable View getPriceHistoryChartForPriceInsightsInfo(PriceInsightsInfo info);
    }

    private final BottomSheetController mBottomSheetController;

    private @Nullable PriceInsightsBottomSheetContent mBottomSheetContent;
    private final PriceInsightsBottomSheetMediator mBottomSheetMediator;
    private final BottomSheetObserver mBottomSheetObserver;
    private final View mPriceInsightsView;
    private @Nullable Long mSheetOpenTimeMs;

    /**
     * @param context The {@link Context} associated with this coordinator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param shoppingService Network service for fetching price insights and price tracking info.
     */
    public PriceInsightsBottomSheetCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            Tab tab,
            TabModelSelector tabModelSelector,
            ShoppingService shoppingService,
            PriceInsightsDelegate priceInsightsDelegate) {
        mBottomSheetController = bottomSheetController;
        PropertyModel propertyModel =
                new PropertyModel(PriceInsightsBottomSheetProperties.ALL_KEYS);
        mPriceInsightsView =
                LayoutInflater.from(context)
                        .inflate(R.layout.price_insights_container, /* root= */ null);
        PropertyModelChangeProcessor.create(
                propertyModel, mPriceInsightsView, PriceInsightsBottomSheetViewBinder::bind);
        mBottomSheetMediator =
                new PriceInsightsBottomSheetMediator(
                        context,
                        tab,
                        tabModelSelector,
                        shoppingService,
                        priceInsightsDelegate,
                        propertyModel);
        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {

                    @Override
                    public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                        if (mSheetOpenTimeMs != null) {
                            long durationMs = SystemClock.elapsedRealtime() - mSheetOpenTimeMs;
                            RecordHistogram.recordTimesHistogram(
                                    "Commerce.PriceInsights.BottomSheetBrowsingTime", durationMs);
                            mSheetOpenTimeMs = null;
                        }
                        if (newContent != mBottomSheetContent) {
                            mBottomSheetController.removeObserver(mBottomSheetObserver);
                        }
                    }
                };
    }

    /** Request to show the price insights bottom sheet. */
    public void requestShowContent() {
        ScrollView scrollView = mPriceInsightsView.findViewById(R.id.scroll_view);
        mBottomSheetContent = new PriceInsightsBottomSheetContent(mPriceInsightsView, scrollView);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mBottomSheetMediator.requestShowContent();
        if (mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            mSheetOpenTimeMs = SystemClock.elapsedRealtime();
            RecordUserAction.record("Commerce.PriceInsights.BottomSheetOpened");
        }
    }

    /** Close the price insights bottom sheet. */
    public void closeContent() {
        mBottomSheetMediator.closeContent();
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }
}
