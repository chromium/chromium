// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;

import androidx.annotation.Px;

import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillViewBase;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.Set;

/**
 * This class is responsible for rendering the bottom sheet which displays the facilitated payments
 * instruments. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class FacilitatedPaymentsPaymentMethodsView extends TouchToFillViewBase {
    /**
     * Constructs a FacilitatedPaymentsPaymentMethodsView which creates, modifies, and shows the
     * bottom sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    FacilitatedPaymentsPaymentMethodsView(
            Context context, BottomSheetController bottomSheetController) {
        super(
                bottomSheetController,
                (RelativeLayout)
                        LayoutInflater.from(context).inflate(R.layout.touch_to_fill_sheet, null),
                true);
    }

    @Override
    public int getVerticalScrollOffset() {
        return getSheetItemListView().computeVerticalScrollOffset();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ok;
    }

    @Override
    protected View getHandlebar() {
        return getContentView().findViewById(R.id.drag_handlebar);
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
        return Set.of(FacilitatedPaymentsPaymentMethodsProperties.ItemType.PAYMENT_INSTRUMENT);
    }

    @Override
    protected int footerItemType() {
        return FacilitatedPaymentsPaymentMethodsProperties.ItemType.FOOTER;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return true;
    }
}
