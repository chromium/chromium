// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.Px;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.touch_to_fill.common.ItemDividerBase;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillViewBase;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillCreditCardProperties.ItemType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This class is responsible for rendering the bottom sheet which displays the
 * TouchToFillCreditCard. It is a View in this Model-View-Controller component and doesn't inherit
 * but holds Android Views.
 */
class TouchToFillCreditCardView extends TouchToFillViewBase {
    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
    private Runnable mScanCreditCardHandler;

    private static class HorizontalDividerItemDecoration extends ItemDividerBase {
        HorizontalDividerItemDecoration(int horizontalMargin, Context context) {
            super(horizontalMargin, context);
        }

        @Override
        protected int selectBackgroundDrawable(
                int position, boolean containsFillButton, int itemCount) {
            return super.selectBackgroundDrawable(position, containsFillButton, itemCount);
        }

        @Override
        protected boolean shouldSkipItemType(@ItemType int type) {
            switch (type) {
                case ItemType.HEADER: // Fallthrough.
                case ItemType.FILL_BUTTON:
                    return true;
                case ItemType.CREDIT_CARD:
                    return false;
            }
            assert false : "Undefined whether to skip setting background for item of type: " + type;
            return true; // Should never be reached. But if, skip to not change anything.
        }

        @Override
        protected boolean containsFillButton(RecyclerView parent) {
            return parent.getAdapter().getItemViewType(parent.getAdapter().getItemCount() - 1)
                    == ItemType.FILL_BUTTON;
        }
    }

    /**
     * Constructs a TouchToFillCreditCardView which creates, modifies, and shows the bottom sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillCreditCardView(Context context, BottomSheetController bottomSheetController) {
        super(bottomSheetController,
                (RelativeLayout) LayoutInflater.from(context).inflate(
                        R.layout.touch_to_fill_credit_card_sheet, null));
        mBottomSheetController = bottomSheetController;
        mSheetItemListView = getItemList();

        mSheetItemListView.addItemDecoration(new HorizontalDividerItemDecoration(
                getContentView().getResources().getDimensionPixelSize(
                        R.dimen.ttf_for_payments_items_spacing),
                context));
    }

    void setScanCreditCardButton(boolean shouldShowScanCreditCard) {
        View scanCreditCard = getContentView().findViewById(R.id.scan_new_card);
        if (shouldShowScanCreditCard) {
            scanCreditCard.setVisibility(View.VISIBLE);
            scanCreditCard.setOnClickListener(unused -> mScanCreditCardHandler.run());
        } else {
            scanCreditCard.setVisibility(View.GONE);
            scanCreditCard.setOnClickListener(null);
        }
    }

    void setScanCreditCardCallback(Runnable callback) {
        mScanCreditCardHandler = callback;
    }

    void setShowCreditCardSettingsCallback(Runnable callback) {
        View managePaymentMethodsButton =
                getContentView().findViewById(R.id.manage_payment_methods);
        managePaymentMethodsButton.setOnClickListener(unused -> callback.run());
    }

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(crbug.com/1247698): Introduce and use proper payments string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/1247698): Introduce and use proper payments string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/1247698): Introduce and use proper payments string.
        return android.R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/1247698): Introduce and use proper payments string.
        return android.R.string.ok;
    }

    @Override
    protected View getHandlebar() {
        return getContentView().findViewById(R.id.drag_handlebar);
    }

    @Override
    protected View getFooter() {
        return getContentView().findViewById(R.id.touch_to_fill_footer);
    }

    @Override
    protected RecyclerView getItemList() {
        return getContentView().findViewById(R.id.sheet_item_list);
    }

    @Override
    protected int getConclusiveMarginHeightPx() {
        return getContentView().getResources().getDimensionPixelSize(
                R.dimen.ttf_for_payments_bottom_padding_button);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return getContentView().getResources().getDimensionPixelSize(
                R.dimen.ttf_for_payments_sheet_padding);
    }

    @Override
    protected int listedItemType() {
        return TouchToFillCreditCardProperties.ItemType.CREDIT_CARD;
    }
}
