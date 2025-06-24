// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.ALL_LOYALTY_CARDS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.HOME_SCREEN;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.ViewFlipper;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.touch_to_fill.common.ItemDividerBase;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillViewBase;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.Set;

/**
 * This class is responsible for rendering the bottom sheet which displays the
 * TouchToFillPaymentMethod. It is a View in this Model-View-Controller component and doesn't
 * inherit but holds Android Views.
 */
@NullMarked
class TouchToFillPaymentMethodView extends TouchToFillViewBase {
    private static class HorizontalDividerItemDecoration extends ItemDividerBase {
        HorizontalDividerItemDecoration(Context context) {
            super(context);
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
                case ItemType.FOOTER: // Fallthrough.
                case ItemType.FILL_BUTTON:
                case ItemType.WALLET_SETTINGS_BUTTON:
                case ItemType.TERMS_LABEL:
                    return true;
                case ItemType.CREDIT_CARD:
                case ItemType.IBAN:
                case ItemType.LOYALTY_CARD:
                case ItemType.ALL_LOYALTY_CARDS:
                    return false;
            }
            assert false : "Undefined whether to skip setting background for item of type: " + type;
            return true; // Should never be reached. But if, skip to not change anything.
        }

        @Override
        protected boolean containsFillButton(RecyclerView parent) {
            int itemCount = assumeNonNull(parent.getAdapter()).getItemCount();
            // The button will be above the footer if it's present.
            return itemCount > 1
                    && parent.getAdapter().getItemViewType(itemCount - 2) == ItemType.FILL_BUTTON;
        }
    }

    /**
     * Constructs a TouchToFillPaymentMethodView which creates, modifies, and shows the bottom sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillPaymentMethodView(Context context, BottomSheetController bottomSheetController) {
        super(
                bottomSheetController,
                (RelativeLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.touch_to_fill_payment_method_sheet, null),
                true);
    }

    void setCurrentScreen(@ScreenId int screenId) {
        ViewFlipper viewFlipper =
                getContentView().findViewById(R.id.touch_to_fill_payment_method_view_flipper);
        viewFlipper.setDisplayedChild(getDisplayedChildForScreenId(screenId));
        setSheetItemListView(getContentView().findViewById(getListViewIdForScreenId(screenId)));
        getSheetItemListView()
                .addItemDecoration(
                        new HorizontalDividerItemDecoration(getContentView().getContext()));
    }

    void setBackPressHandler(Runnable backPressHandler) {
        getContentView()
                .findViewById(R.id.all_loyalty_cards_back_image_button)
                .setOnClickListener(
                        (unused) -> {
                            backPressHandler.run();
                            // TODO: crbug.com/420957826 - Remeasure the bottom sheet.
                        });
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        // TODO - crbug.com/: Update for loyalty cards.
        return context.getString(R.string.autofill_payment_method_bottom_sheet_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // TODO - crbug.com/: Update for loyalty cards.
        return R.string.autofill_payment_method_bottom_sheet_half_height;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        // TODO - crbug.com/: Update for loyalty cards.
        return R.string.autofill_payment_method_bottom_sheet_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        // TODO - crbug.com/: Update for loyalty cards.
        return R.string.autofill_payment_method_bottom_sheet_closed;
    }

    @Override
    protected View getHandlebar() {
        return getContentView().findViewById(R.id.drag_handlebar);
    }

    @Override
    protected @Nullable View getHeaderView() {
        ViewFlipper viewFlipper =
                getContentView().findViewById(R.id.touch_to_fill_payment_method_view_flipper);
        if (viewFlipper.getDisplayedChild()
                == getDisplayedChildForScreenId(ALL_LOYALTY_CARDS_SCREEN)) {
            // Only the all loyalty cards screen has a static header;
            return getContentView().findViewById(R.id.all_loyalty_cards_toolbar);
        }
        return null;
    }

    @Override
    protected int getConclusiveMarginHeightPx() {
        return getContentView().getResources().getDimensionPixelSize(R.dimen.ttf_sheet_padding);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return getContentView().getResources().getDimensionPixelSize(R.dimen.ttf_sheet_padding);
    }

    @Override
    protected Set<Integer> listedItemTypes() {
        return Set.of(
                TouchToFillPaymentMethodProperties.ItemType.CREDIT_CARD,
                TouchToFillPaymentMethodProperties.ItemType.IBAN,
                TouchToFillPaymentMethodProperties.ItemType.LOYALTY_CARD);
    }

    @Override
    protected int footerItemType() {
        return TouchToFillPaymentMethodProperties.ItemType.FOOTER;
    }

    private int getDisplayedChildForScreenId(@ScreenId int screenId) {
        switch (screenId) {
            case HOME_SCREEN:
                return 0;
            case ALL_LOYALTY_CARDS_SCREEN:
                return 1;
        }
        assert false : "Undefined ScreenId: " + screenId;
        return 0;
    }

    private @IdRes int getListViewIdForScreenId(@ScreenId int screenId) {
        switch (screenId) {
            case HOME_SCREEN:
                return R.id.touch_to_fill_payment_method_home_screen;
            case ALL_LOYALTY_CARDS_SCREEN:
                return R.id.touch_to_fill_all_loyalty_cards_list;
        }
        assert false : "Undefined ScreenId: " + screenId;
        return 0;
    }
}
