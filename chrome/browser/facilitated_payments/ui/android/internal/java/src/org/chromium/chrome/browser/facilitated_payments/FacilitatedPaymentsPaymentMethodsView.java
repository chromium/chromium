// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This class is responsible for rendering the bottom sheet which displays the FacilitatedPayments
 * instruments. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class FacilitatedPaymentsPaymentMethodsView implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final View mView;

    private RecyclerView mSheetItemListView;

    FacilitatedPaymentsPaymentMethodsView(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mView = new View(context);
        mSheetItemListView = new RecyclerView(context);
    }

    public boolean setVisible(boolean isVisible) {
        if (isVisible) {
            if (!mBottomSheetController.requestShowContent(this, true)) {
                return false;
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
        return true;
    }

    @Override
    public View getContentView() {
        return mView;
    }

    public void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
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
    public boolean hasCustomLifecycle() {
        return true;
    }
}
