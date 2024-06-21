// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.LocalizationUtils;

/**
 * This class is responsible for rendering the bottom sheet which displays the facilitated payments
 * instruments. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class FacilitatedPaymentsPaymentMethodsView implements BottomSheetContent {
    private final LinearLayout mView;
    private final FrameLayout mScreenHolder;
    private final RecyclerView mSheetItemListView;
    private final BottomSheetController mBottomSheetController;
    private Callback<Integer> mDismissHandler;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    super.onSheetClosed(reason);
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    /**
     * Constructs a FacilitatedPaymentsPaymentMethodsView which creates, modifies, and shows the
     * bottom sheet.
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    FacilitatedPaymentsPaymentMethodsView(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
        mView =
                (LinearLayout)
                        LayoutInflater.from(context)
                                .inflate(R.layout.facilitated_payments_sequence_view, null);
        mScreenHolder = (FrameLayout) mView.findViewById(R.id.screen_holder);
        mSheetItemListView =
                (RecyclerView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.facilitated_payments_fop_selector,
                                        mScreenHolder,
                                        false);
        mScreenHolder.addView(mSheetItemListView);

        mSheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false) {
                    @Override
                    public boolean isAutoMeasureEnabled() {
                        return true;
                    }

                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            RecyclerView.Recycler recycler,
                            RecyclerView.State state,
                            AccessibilityNodeInfoCompat info) {}
                });

        // Apply RTL layout changes.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mView.setLayoutDirection(layoutDirection);
    }

    /**
     * Sets the {@link RecyclerView.Adapter} for the {@link RecyclerView}.
     *
     * @param adapter The {@link RecyclerView.Adapter} to add items to the view.
     */
    public void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     *
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @return True if the request was successful, false otherwise
     */
    public boolean setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, /* animate= */ true)) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                return false;
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
        return true;
    }

    /**
     * Sets a new listener that reacts to bottom sheet dismissal.
     *
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    public void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    @Override
    public View getContentView() {
        return mView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ok;
    }
}
