// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_insights;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;

/**
 * Responsible for providing UI resources for showing price insights action on optional toolbar
 * button.
 */
@NullMarked
public class PriceInsightsButtonController extends BaseButtonDataProvider {

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Supplier<ShoppingService> mShoppingServiceSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final PriceInsightsDelegate mPriceInsightsDelegate;
    private @Nullable PriceInsightsBottomSheetCoordinator mBottomSheetCoordinator;
    private @Nullable PriceInsightsBottomSheetCoordinator mBottomSheetCoordinatorForTesting;

    Supplier<CommerceBottomSheetContentController> mCommerceBottomSheetContentController;

    public PriceInsightsButtonController(
            Context context,
            Supplier<@Nullable Tab> tabSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            Supplier<ShoppingService> shoppingServiceSupplier,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            SnackbarManager snackbarManager,
            PriceInsightsDelegate priceInsightsDelegate,
            Drawable buttonDrawable,
            Supplier<CommerceBottomSheetContentController> commerceBottomSheetContentController) {
        super(
                tabSupplier,
                modalDialogManager,
                buttonDrawable,
                /* contentDescription= */ context.getString(R.string.price_insights_title),
                /* actionChipLabelResId= */ R.string.price_insights_price_is_low_title,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                /* tooltipTextResId= */ Resources.ID_NULL);

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
        ShoppingService shoppingService = mShoppingServiceSupplier.get();
        if (shoppingService == null) {
            showErrorToastMessage();
            return;
        }
        if (CommerceFeatureUtils.isDiscountInfoApiEnabled(shoppingService)) {
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
                Tab tab = mTabSupplier.get();
                if (tab == null) {
                    showErrorToastMessage();
                    return;
                }
                mBottomSheetCoordinator =
                        new PriceInsightsBottomSheetCoordinator(
                                mContext,
                                mBottomSheetController,
                                tab,
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
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        IphCommandBuilder iphCommandBuilder =
                new IphCommandBuilder(
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

    private void showErrorToastMessage() {
        @StringRes int textResId = R.string.price_insights_content_price_tracking_error_message;
        Toast.makeText(mContext, textResId, Toast.LENGTH_SHORT).show();
    }
}
