// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

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
            @NonNull ShoppingService shoppingService) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        PropertyModel propertyModel =
                new PropertyModel(PriceInsightsBottomSheetProperties.ALL_KEYS);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        propertyModel,
                        mPriceInsightsView,
                        PriceInsightsBottomSheetViewBinder::bind);
        mPriceInsightsView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.price_insights_container, /* root= */ null);
        mBottomSheetMediator =
                new PriceInsightsBottomSheetMediator(mContext, shoppingService, propertyModel);
    }

    public void requestShowContent() {
        mBottomSheetContent = new PriceInsightsBottomSheetContent(mPriceInsightsView);
        mBottomSheetMediator.requestShowContent();
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    public void closeContent() {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ true);
    }

    void setMediatorForTesting(PriceInsightsBottomSheetMediator mockMediator) {
        mBottomSheetMediator = mockMediator;
    }
}
