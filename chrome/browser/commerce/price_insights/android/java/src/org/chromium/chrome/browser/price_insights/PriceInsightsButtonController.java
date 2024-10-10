// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Responsible for providing UI resources for showing price insights action on optional toolbar
 * button.
 */
public class PriceInsightsButtonController extends BaseButtonDataProvider {

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<Tab> mTabSupplier;
    private final PriceInsightsDelegate mPriceInsightsDelegate;
    private PriceInsightsBottomSheetCoordinator mBottomSheetCoordinator;
    private PriceInsightsBottomSheetCoordinator mBottomSheetCoordinatorForTesting;

    @NonNull Supplier<CommerceBottomSheetContentController> mCommerceBottomSheetContentController;

    public PriceInsightsButtonController(
            Context context,
            Supplier<Tab> tabSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<ShoppingService> shoppingServiceSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            PriceInsightsDelegate priceInsightsDelegate,
            Drawable buttonDrawable,
            @NonNull
                    Supplier<CommerceBottomSheetContentController>
                            commerceBottomSheetContentController) {
        super(
                tabSupplier,
                modalDialogManager,
                buttonDrawable,
                /* contentDescriptionResId= */ context.getString(R.string.price_insights_title),
                /* actionChipLabelResId= */ R.string.price_insights_price_is_low_title,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                /* tooltipTextResId= */ Resources.ID_NULL,
                /* showHoverHighlight= */ false);

        mContext = context;
        mBottomSheetController = bottomSheetController;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mShoppingServiceSupplier = shoppingServiceSupplier;
        mTabSupplier = tabSupplier;
        mPriceInsightsDelegate = priceInsightsDelegate;

        mBottomSheetObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetStateChanged(int newState, int reason) {
                        mButtonData.setEnabled(newState == SheetState.HIDDEN);
                        notifyObservers(mButtonData.canShow());
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mCommerceBottomSheetContentController = commerceBottomSheetContentController;
    }

    @Override
    public void onClick(View view) {
        if (ChromeFeatureList.sEnableDiscountInfoApi.isEnabled()) {
            assert mCommerceBottomSheetContentController.get() != null;
            mCommerceBottomSheetContentController.get().requestShowContent();
        } else {
            // Close content and destroy previous coordinator.
            if (mBottomSheetCoordinator != null) {
                mBottomSheetCoordinator.closeContent();
                mBottomSheetCoordinator = null;
            }

            // Create a new coordinator and show content.
            if (mBottomSheetCoordinatorForTesting != null) {
                mBottomSheetCoordinator = mBottomSheetCoordinatorForTesting;
            } else {
                mBottomSheetCoordinator =
                        new PriceInsightsBottomSheetCoordinator(
                                mContext,
                                mBottomSheetController,
                                mTabSupplier.get(),
                                mTabModelSelectorSupplier.get(),
                                mShoppingServiceSupplier.get(),
                                mPriceInsightsDelegate);
            }
            mBottomSheetCoordinator.requestShowContent();
        }
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mBottomSheetCoordinator != null) {
            mBottomSheetCoordinator.closeContent();
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                        tab.getContext().getResources(),
                        FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_QUIET_VARIANT,
                        /* stringId= */ R.string.price_insights_price_is_low_title,
                        /* accessibilityStringId= */ R.string.price_insights_price_is_low_title);
        return iphCommandBuilder;
    }

    void setPriceInsightsBottomSheetCoordinatorForTesting(
            PriceInsightsBottomSheetCoordinator coordinator) {
        mBottomSheetCoordinatorForTesting = coordinator;
        ResettersForTesting.register(() -> mBottomSheetCoordinatorForTesting = null);
    }
}
