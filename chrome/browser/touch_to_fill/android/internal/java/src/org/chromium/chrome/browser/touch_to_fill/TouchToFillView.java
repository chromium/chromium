// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class TouchToFillView implements BottomSheetContent {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
    private final LinearLayout mContentView;
    private Callback<Integer> mDismissHandler;

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
            super.onSheetClosed(reason);
            assert mDismissHandler != null;
            mDismissHandler.onResult(reason);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }

        @Override
        public void onSheetStateChanged(int newState, int reason) {
            super.onSheetStateChanged(newState, reason);
            if (newState != BottomSheetController.SheetState.HIDDEN) return;
            // This is a fail-safe for cases where onSheetClosed isn't triggered.
            mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
    };

    /**
     * Constructs a TouchToFillView which creates, modifies, and shows the bottom sheet.
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillView(Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.touch_to_fill_sheet, null);
        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(new LinearLayoutManager(
                mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSheetItemListView.setItemAnimator(null);
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @return True if the request was successful, false otherwise.
     */
    boolean setVisible(boolean isVisible) {
        if (isVisible) {
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, true)) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                return false;
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
        return true;
    }

    void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    void setOnManagePasswordClick(Runnable runnable) {
        mContentView.findViewById(R.id.touch_to_fill_sheet_manage_passwords)
                .setOnClickListener((v) -> runnable.run());
    }

    Context getContext() {
        return mContext;
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return Math.min(getDesiredSheetHeight(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.touch_to_fill_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.touch_to_fill_sheet_closed;
    }

    // TODO(crbug.com/1009331): This should add up the height of all items up to the 2nd credential.
    private @Px int getDesiredSheetHeight() {
        Resources resources = mContext.getResources();
        @Px
        int totalHeight = resources.getDimensionPixelSize(
                R.dimen.touch_to_fill_sheet_height_single_credential);

        final boolean hasMultipleCredentials = mSheetItemListView.getAdapter() != null
                && mSheetItemListView.getAdapter().getItemCount() > 2
                && mSheetItemListView.getAdapter().getItemViewType(2)
                        == TouchToFillProperties.ItemType.CREDENTIAL;
        if (hasMultipleCredentials) {
            totalHeight += resources.getDimensionPixelSize(
                    R.dimen.touch_to_fill_sheet_height_second_credential);
            totalHeight += resources.getDimensionPixelSize(
                    R.dimen.touch_to_fill_sheet_bottom_padding_credentials);
        } else {
            totalHeight +=
                    resources.getDimensionPixelSize(R.dimen.touch_to_fill_sheet_height_button);
            totalHeight += resources.getDimensionPixelSize(
                    R.dimen.touch_to_fill_sheet_bottom_padding_button);
        }

        return totalHeight;
    }
}
