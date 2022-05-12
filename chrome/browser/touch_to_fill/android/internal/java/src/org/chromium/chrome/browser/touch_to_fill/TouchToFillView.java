// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerUI;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
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

    private static class HorizontalDividerItemDecoration extends RecyclerView.ItemDecoration {
        private final int mHorizontalMargin;
        private final Context mContext;

        HorizontalDividerItemDecoration(int horizontalMargin, Context context) {
            this.mHorizontalMargin = horizontalMargin;
            this.mContext = context;
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            outRect.top = getItemOffsetInternal(view, parent, state);
        }

        private int getItemOffsetInternal(
                final View view, final RecyclerView parent, RecyclerView.State state) {
            return mHorizontalMargin;
        }

        /**
         * Returns the proper background for each of the credential items depending on their
         * position.
         * @param position Position of the credential inside the list, including the header and the
         *         button.
         * @param containsFillButton Indicates if the fill button is in the list.
         * @param itemCount Shows how many items are in the list, including the header and the
         *         button.
         * @return The ID of the selected background resource.
         */
        private int selectBackgroundDrawable(
                int position, boolean containsFillButton, int itemCount) {
            if (!usesUnifiedPasswordManagerUI()) {
                return R.drawable.touch_to_fill_credential_background;
            }
            if (containsFillButton) { // Round all the corners of the only item.
                return R.drawable.touch_to_fill_credential_background_modern_rounded_all;
            }
            if (position == 1) { // Round the top of the first item.
                return R.drawable.touch_to_fill_credential_background_modern_rounded_up;
            }
            if (position == itemCount - 1) { // Round the bottom of the last item.
                return R.drawable.touch_to_fill_credential_background_modern_rounded_down;
            }
            // The rest of the items have a background with no rounded edges.
            return R.drawable.touch_to_fill_credential_background_modern;
        }

        @Override
        public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
            int itemCount = state.getItemCount();
            boolean containsFillButton = parent.getAdapter().getItemViewType(itemCount - 1)
                    == TouchToFillProperties.ItemType.FILL_BUTTON;
            if (containsFillButton) {
                // The background of the button should not be changed.
                itemCount--;
            }
            // Skipping the first item because it's the header.
            for (int i = 1; i < itemCount; i++) {
                View child = parent.getChildAt(i);
                int position = parent.getChildAdapterPosition(child);
                child.setBackground(AppCompatResources.getDrawable(mContext,
                        selectBackgroundDrawable(position, containsFillButton, itemCount)));
            }
        }
    }

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
                usesUnifiedPasswordManagerUI() ? R.layout.touch_to_fill_sheet_modern
                                               : R.layout.touch_to_fill_sheet,
                null);
        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(new LinearLayoutManager(
                mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false));
        if (usesUnifiedPasswordManagerUI()) {
            mSheetItemListView.addItemDecoration(new HorizontalDividerItemDecoration(
                    mContentView.getResources().getDimensionPixelSize(
                            R.dimen.touch_to_fill_sheet_items_spacing),
                    mContext));
        }
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
        int totalHeight = resources.getDimensionPixelSize(usesUnifiedPasswordManagerUI()
                        ? R.dimen.touch_to_fill_sheet_height_single_credential_modern
                        : R.dimen.touch_to_fill_sheet_height_single_credential);

        final boolean hasMultipleCredentials = mSheetItemListView.getAdapter() != null
                && mSheetItemListView.getAdapter().getItemCount() > 2
                && mSheetItemListView.getAdapter().getItemViewType(2)
                        == TouchToFillProperties.ItemType.CREDENTIAL;
        if (hasMultipleCredentials) {
            totalHeight += getSecondCredentialAndSpacingHeight(resources);
        } else {
            totalHeight += getButtonAndSpacingHeight(resources);
        }

        return totalHeight;
    }

    /**
     * Calculates the height of the button together with the padding.
     */
    private @Px int getSecondCredentialAndSpacingHeight(Resources resources) {
        int secondCredentialHeight;
        int secondCredentialPadding;
        if (usesUnifiedPasswordManagerUI()) {
            secondCredentialHeight = R.dimen.touch_to_fill_sheet_height_second_credential_modern;
            secondCredentialPadding = R.dimen.touch_to_fill_sheet_bottom_padding_credentials_modern;
        } else {
            secondCredentialHeight = R.dimen.touch_to_fill_sheet_height_second_credential;
            secondCredentialPadding = R.dimen.touch_to_fill_sheet_bottom_padding_credentials;
        }
        return resources.getDimensionPixelSize(secondCredentialHeight)
                + resources.getDimensionPixelSize(secondCredentialPadding);
    }

    /**
     * Calculates the height of the second credential together with the padding.
     */
    private @Px int getButtonAndSpacingHeight(Resources resources) {
        int buttonHeight;
        int buttonPadding;
        if (usesUnifiedPasswordManagerUI()) {
            buttonHeight = R.dimen.touch_to_fill_sheet_height_button_modern;
            buttonPadding = R.dimen.touch_to_fill_sheet_bottom_padding_button_modern;
        } else {
            buttonHeight = R.dimen.touch_to_fill_sheet_height_button;
            buttonPadding = R.dimen.touch_to_fill_sheet_bottom_padding_button;
        }
        return resources.getDimensionPixelSize(buttonHeight)
                + resources.getDimensionPixelSize(buttonPadding);
    }
}
