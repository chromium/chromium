// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.payments;

import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.ALL_LOYALTY_CARDS_SCREEN;
import static org.chromium.chrome.browser.touch_to_fill.payments.TouchToFillPaymentMethodProperties.ScreenId.HOME_SCREEN;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.RelativeLayout;
import android.widget.ViewFlipper;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

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

    private @StringRes int mSheetContentDescriptionId;
    private @StringRes int mSheetFullHeightDescriptionId;
    private @StringRes int mSheetHalfHeightDescriptionId;
    private @StringRes int mSheetClosedDescriptionId;

    private static class HorizontalDividerItemDecoration extends ItemDividerBase {
        HorizontalDividerItemDecoration(Context context) {
            super(context);
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

    void setFocusedViewIdForAccessibility(@IdRes int focusedViewIdForAccessibility) {
        View view = getContentView().findViewById(focusedViewIdForAccessibility);
        view.requestFocus();
        view.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    void setBackPressHandler(Runnable backPressHandler) {
        getContentView()
                .findViewById(R.id.all_loyalty_cards_back_image_button)
                .setOnClickListener((unused) -> backPressHandler.run());
    }

    public void setSheetContentDescriptionId(@StringRes int sheetContentDescriptionId) {
        mSheetContentDescriptionId = sheetContentDescriptionId;
    }

    public void setSheetHalfHeigthDescriptionId(@StringRes int sheetHalfHeightDescriptionId) {
        mSheetHalfHeightDescriptionId = sheetHalfHeightDescriptionId;
    }

    public void setSheetFullHeightDescriptionId(@StringRes int sheetFullHeightDescriptionId) {
        mSheetFullHeightDescriptionId = sheetFullHeightDescriptionId;
    }

    public void setSheetClosedDescriptionId(@StringRes int sheetClosedDescriptionId) {
        mSheetClosedDescriptionId = sheetClosedDescriptionId;
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public @NonNull String getSheetContentDescription(Context context) {
        return getContentView().getContext().getString(mSheetContentDescriptionId);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return mSheetHalfHeightDescriptionId;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return mSheetFullHeightDescriptionId;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return mSheetClosedDescriptionId;
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
