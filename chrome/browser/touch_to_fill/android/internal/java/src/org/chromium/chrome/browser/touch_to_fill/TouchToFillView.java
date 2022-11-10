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
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ItemType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;

/**
 * This class is responsible for rendering the bottom sheet which displays the touch to fill
 * credentials. It is a View in this Model-View-Controller component and doesn't inherit but holds
 * Android Views.
 */
class TouchToFillView implements BottomSheetContent {
    private static final int MAX_FULLY_VISIBLE_CREDENTIAL_COUNT = 2;

    private final BottomSheetController mBottomSheetController;
    private final RecyclerView mSheetItemListView;
    private final RelativeLayout mContentView;
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
        mBottomSheetController = bottomSheetController;
        mContentView = (RelativeLayout) LayoutInflater.from(context).inflate(
                R.layout.touch_to_fill_sheet, null);
        mSheetItemListView = mContentView.findViewById(R.id.sheet_item_list);
        mSheetItemListView.setLayoutManager(new LinearLayoutManager(
                mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false) {
            @Override
            public boolean isAutoMeasureEnabled() {
                return true;
            }
        });

        // Apply RTL layout changes.
        int layoutDirection = LocalizationUtils.isLayoutRtl() ? View.LAYOUT_DIRECTION_RTL
                                                              : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);

        if (usesUnifiedPasswordManagerBranding()) {
            mSheetItemListView.addItemDecoration(new HorizontalDividerItemDecoration(
                    mContentView.getResources().getDimensionPixelSize(
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

    void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        mSheetItemListView.setAdapter(adapter);
    }

    void setOnManagePasswordClick(Runnable runnable) {
        mContentView.findViewById(R.id.touch_to_fill_sheet_manage_passwords)
                .setOnClickListener((v) -> runnable.run());
    }

    void setManagePasswordText(String buttonText) {
        TextView view = mContentView.findViewById(R.id.touch_to_fill_sheet_manage_passwords);
        view.setText(buttonText);
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
    public float getFullHeightRatio() {
        // WRAP_CONTENT would be the right fit but this disables the HALF state.
        return Math.min(getMaximumSheetHeight(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
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

    /**
     * Returns the height of the half state. Does not show Manage passwords. For 1 suggestion  (plus
     * fill button) or 2 suggestions, it shows all items fully. For 3+ suggestions, it shows the
     * first 2.5 suggestion to encourage scrolling.
     *
     * @return the half state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getDesiredSheetHeight() {
        if (mSheetItemListView.getAdapter() == null) {
            // TODO(crbug.com/1330933): Assert this condition in setVisible. Should never happen.
            return HeightMode.DEFAULT;
        }
        return getHeightWithMargins(mContentView.findViewById(R.id.drag_handlebar), false)
                + getSheetItemListHeightWithMargins(true);
    }

    /**
     * Returns the height of the full state. Must show Manage passwords permanently. For up to three
     * suggestions, the sheet usually cannot fill the screen.
     *
     * @return the full state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    private @Px int getMaximumSheetHeight() {
        if (mSheetItemListView.getAdapter() == null) {
            // TODO(crbug.com/1330933): Assert this condition in setVisible. Should never happen.
            return HeightMode.DEFAULT;
        }
        @Px
        int requiredMaxHeight = getHeightWhenFullyExtended();
        if (requiredMaxHeight <= mBottomSheetController.getContainerHeight()) {
            return requiredMaxHeight;
        }
        // If the footer would move off-screen, make it sticky and update the layout.
        remeasure(true);
        ViewUtils.requestLayout(mContentView, "TouchToFillView.getMaximumSheetHeight");
        return getHeightWhenFullyExtended();
    }

    private @Px int getHeightWhenFullyExtended() {
        assert mContentView.getMeasuredHeight() > 0 : "ContentView hasn't been measured.";
        int height = getHeightWithMargins(mContentView.findViewById(R.id.drag_handlebar), false)
                + getSheetItemListHeightWithMargins(false);
        View footer = mContentView.findViewById(R.id.touch_to_fill_footer);
        if (footer.getMeasuredHeight() == 0) {
            // This can happen when bottom sheet container layout changes, apparently causing
            // the footer layout to be invalidated. Measure the content view again.
            remeasure(true);
            ViewUtils.requestLayout(footer, "TouchToFillView.getHeightWhenFullyExtended");
        }
        height += getHeightWithMargins(footer, false);
        return height;
    }

    private @Px int getSheetItemListHeightWithMargins(boolean showOnlyInitialItems) {
        assert mSheetItemListView.getMeasuredHeight() > 0 : "Sheet item list hasn't been measured.";
        @Px
        int totalHeight = 0;
        int visibleItems = 0;
        for (int posInSheet = 0; posInSheet < mSheetItemListView.getChildCount(); posInSheet++) {
            View child = mSheetItemListView.getChildAt(posInSheet);
            if (isCredential(child)) visibleItems++;
            if (showOnlyInitialItems && visibleItems > MAX_FULLY_VISIBLE_CREDENTIAL_COUNT) {
                // If the current item is the last to be shown, skip remaining elements and margins.
                totalHeight += getHeightWithMargins(child, true);
                return totalHeight;
            }
            totalHeight += getHeightWithMargins(child, false);
        }
        // Since the last element is fully visible, add the conclusive margin.
        totalHeight += getContentView().getResources().getDimensionPixelSize(
                usesUnifiedPasswordManagerBranding()
                        ? R.dimen.touch_to_fill_sheet_bottom_padding_button_modern
                        : R.dimen.touch_to_fill_sheet_bottom_padding_button);
        return totalHeight;
    }

    private boolean isCredential(View childInSheetView) {
        int posInAdapter = mSheetItemListView.getChildAdapterPosition(childInSheetView);
        return mSheetItemListView.getAdapter().getItemViewType(posInAdapter)
                == TouchToFillProperties.ItemType.CREDENTIAL;
    }

    private static @Px int getHeightWithMargins(View view, boolean shouldPeek) {
        assert view.getMeasuredHeight() > 0 : "View hasn't been measured.";
        return getMargins(view, /*excludeBottomMargin=*/shouldPeek)
                + (shouldPeek ? view.getMeasuredHeight() / 2 : view.getMeasuredHeight());
    }

    private static @Px int getMargins(View view, boolean excludeBottomMargin) {
        LayoutParams params = view.getLayoutParams();
        if (params instanceof MarginLayoutParams) {
            MarginLayoutParams marginParams = (MarginLayoutParams) params;
            return marginParams.topMargin + (excludeBottomMargin ? 0 : marginParams.bottomMargin);
        }
        return 0;
    }

    private void remeasure(boolean useStickyFooter) {
        RelativeLayout.LayoutParams footerLayoutParams =
                (RelativeLayout.LayoutParams) mContentView.findViewById(R.id.touch_to_fill_footer)
                        .getLayoutParams();
        RelativeLayout.LayoutParams sheetItemListLayoutParams =
                (RelativeLayout.LayoutParams) mSheetItemListView.getLayoutParams();
        if (useStickyFooter) {
            sheetItemListLayoutParams.addRule(RelativeLayout.ABOVE, R.id.touch_to_fill_footer);
            footerLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
            footerLayoutParams.removeRule(RelativeLayout.BELOW);
        } else {
            sheetItemListLayoutParams.removeRule(RelativeLayout.ABOVE);
            footerLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
            footerLayoutParams.addRule(RelativeLayout.BELOW, R.id.sheet_item_list);
        }
        mContentView.measure(
                View.MeasureSpec.makeMeasureSpec(getInsetDisplayWidth(), MeasureSpec.AT_MOST),
                MeasureSpec.UNSPECIFIED);
        mSheetItemListView.measure(
                View.MeasureSpec.makeMeasureSpec(getInsetDisplayWidth(), MeasureSpec.AT_MOST),
                MeasureSpec.UNSPECIFIED);
    }

    private @Px int getInsetDisplayWidth() {
        return mContentView.getContext().getResources().getDisplayMetrics().widthPixels
                - 2
                * mContentView.getResources().getDimensionPixelSize(
                        usesUnifiedPasswordManagerBranding()
                                ? R.dimen.touch_to_fill_sheet_margin_modern
                                : R.dimen.touch_to_fill_sheet_margin);
    }
}
