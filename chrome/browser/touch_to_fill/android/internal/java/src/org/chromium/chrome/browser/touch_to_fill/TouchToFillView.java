// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerBranding;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.chrome.browser.touch_to_fill.common.TouchToFillViewBase;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class TouchToFillView extends TouchToFillViewBase {
    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
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
         *
         * @param position Position of the credential inside the list, including the header and the
         *         button.
         * @param containsFillButton Indicates if the fill button is in the list.
         * @param itemCount Shows how many items are in the list, including the header and the
         *         button.
         * @return The ID of the selected background resource.
         */
        private int selectBackgroundDrawable(
                int position, boolean containsFillButton, int itemCount) {
            if (!usesUnifiedPasswordManagerBranding()) {
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
            for (int posInView = 0; posInView < parent.getChildCount(); posInView++) {
                View child = parent.getChildAt(posInView);
                int posInAdapter = parent.getChildAdapterPosition(child);
                if (shouldSkipItemType(parent.getAdapter().getItemViewType(posInAdapter))) continue;
                child.setBackground(AppCompatResources.getDrawable(mContext,
                        selectBackgroundDrawable(posInAdapter, containsFillButton(parent),
                                parent.getAdapter().getItemCount())));
            }
        }

        private static boolean shouldSkipItemType(@ItemType int type) {
            switch (type) {
                case ItemType.HEADER: // Fallthrough.
                case ItemType.FILL_BUTTON:
                    return true;
                case ItemType.CREDENTIAL: // Fallthrough.
                case ItemType.WEBAUTHN_CREDENTIAL:
                    return false;
            }
            assert false : "Undefined whether to skip setting background for item of type: " + type;
            return true; // Should never be reached. But if, skip to not change anything.
        }

        private static boolean containsFillButton(RecyclerView parent) {
            return parent.getAdapter().getItemViewType(parent.getAdapter().getItemCount() - 1)
                    == ItemType.FILL_BUTTON;
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
     *
     * @param context A {@link Context} used to load resources and inflate the sheet.
     * @param bottomSheetController The {@link BottomSheetController} used to show/hide the sheet.
     */
    TouchToFillView(Context context, BottomSheetController bottomSheetController) {
        super(bottomSheetController,
                (RelativeLayout) LayoutInflater.from(context).inflate(
                        R.layout.touch_to_fill_sheet, null));
        mBottomSheetController = bottomSheetController;
        mSheetItemListView = getItemList();

        if (usesUnifiedPasswordManagerBranding()) {
            mSheetItemListView.addItemDecoration(new HorizontalDividerItemDecoration(
                    getContentView().getResources().getDimensionPixelSize(
                            R.dimen.touch_to_fill_sheet_items_spacing),
                    context));
        }
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     *
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     *
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @return True if the request was successful, false otherwise.
     */
    boolean setVisible(boolean isVisible) {
        if (isVisible) {
            remeasure(false);
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

    void setOnManagePasswordClick(Runnable runnable) {
        getContentView()
                .findViewById(R.id.touch_to_fill_sheet_manage_passwords)
                .setOnClickListener((v) -> runnable.run());
    }

    void setManagePasswordText(String buttonText) {
        TextView view = getContentView().findViewById(R.id.touch_to_fill_sheet_manage_passwords);
        view.setText(buttonText);
    }

    // TODO(crbug.com/1247698): Move this method to the base class.
    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    @Override
    public int getVerticalScrollOffset() {
        return mSheetItemListView.computeVerticalScrollOffset();
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
    protected @Px int getConclusiveMarginHeightPx() {
        return getContentView().getResources().getDimensionPixelSize(
                usesUnifiedPasswordManagerBranding()
                        ? R.dimen.touch_to_fill_sheet_bottom_padding_button_modern
                        : R.dimen.touch_to_fill_sheet_bottom_padding_button);
    }

    @Override
    protected @Px int getSideMarginPx() {
        return getContentView().getResources().getDimensionPixelSize(
                usesUnifiedPasswordManagerBranding() ? R.dimen.touch_to_fill_sheet_margin_modern
                                                     : R.dimen.touch_to_fill_sheet_margin);
    }

    @Override
    protected int listedItemType() {
        return TouchToFillProperties.ItemType.CREDENTIAL;
    }
}
