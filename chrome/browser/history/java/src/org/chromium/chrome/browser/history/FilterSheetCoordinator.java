// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Coordinator class of the filter bottom sheet UI for history page. */
@NullMarked
class FilterSheetCoordinator implements View.OnLayoutChangeListener {
    // Maximum number of filter items shown on the sheet at once if screen dimension allows.
    static final int MAX_VISIBLE_ITEM_COUNT = 5;

    // Maximum ratio of the sheet height against the base view height.
    static final float MAX_SHEET_HEIGHT_RATIO = 0.7f;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final FilterSheetMediator mMediator;
    private final RecyclerView mItemListView;
    private final BottomSheetContent mSheetContent;
    private final PropertyModel mCloseButtonModel;

    private final View mContentView;
    private final View mBaseView;

    private final CloseCallback mCloseCallback;
    private int mItemCount;

    private int mBaseViewHeight;

    /** Data class for individual item in the filter list. */
    public static class FilterItem {
        public final @Nullable String id;
        public final @Nullable Drawable icon;
        public final CharSequence label;

        public FilterItem(@Nullable String id, @Nullable Drawable icon, CharSequence label) {
            this.id = id;
            this.icon = icon;
            this.label = label;
        }

        /** Return whether the filter item object is valid. */
        public boolean isValid() {
            return id != null;
        }

        @Override
        public boolean equals(Object o) {
            if (o == this) return true;
            return (o instanceof FilterItem filterItem)
                    ? TextUtils.equals(id, filterItem.id)
                    : false;
        }

        @Override
        public int hashCode() {
            return java.util.Objects.hashCode(id);
        }
    }

    /** Callback to be invoked when the sheet gets closed with updated filter item. */
    public interface CloseCallback {
        /**
         * @param filterItem {@link FilterItem} containing the item information. May be {@code null}
         *     if no item is selected.
         */
        void onFilterItemUpdated(@Nullable FilterItem filterItem);
    }

    /**
     * Constructor.
     *
     * @param context {@link Context} for resources, views.
     * @param baseView Base view on which the sheet is opened.
     * @param bottomSheetController {@link BottomSheetController} to open/close the sheet.
     * @param closeCallback Callback invoked when the sheet is closed
     * @param filterItemList List of the items to display in the sheet.
     * @param titleResId Resource ID for the header title of the bottom sheet.
     */
    FilterSheetCoordinator(
            Context context,
            View baseView,
            BottomSheetController bottomSheetController,
            CloseCallback closeCallback,
            List<FilterItem> filterItemList,
            @StringRes int titleResId) {
        mContext = context;
        mBaseView = baseView;
        mBaseViewHeight = mBaseView.getHeight();
        mBottomSheetController = bottomSheetController;
        mCloseCallback = closeCallback;
        var layoutInflater = LayoutInflater.from(context);
        mContentView = layoutInflater.inflate(R.layout.filter_sheet_content, null);
        mItemListView = (RecyclerView) mContentView.findViewById(R.id.filter_item_list);
        mSheetContent =
                new FilterSheetContent(
                        context, mContentView, mItemListView, this::destroy, titleResId);

        ModelList listItems = new ModelList();
        var adapter = new SimpleRecyclerViewAdapter(listItems);
        adapter.registerType(
                0,
                (parent) ->
                        layoutInflater.inflate(
                                R.layout.modern_list_item_small_icon_view, parent, false),
                FilterSheetViewBinder::bind);
        mItemListView.setAdapter(adapter);

        // Close button at the bottom.
        View closeButton = mContentView.findViewById(R.id.close_button);
        mCloseButtonModel =
                new PropertyModel.Builder(FilterSheetProperties.CLOSE_BUTTON_KEY)
                        .with(
                                FilterSheetProperties.CLOSE_BUTTON_CALLBACK,
                                v -> mBottomSheetController.hideContent(mSheetContent, true))
                        .build();
        PropertyModelChangeProcessor.create(
                mCloseButtonModel, closeButton, FilterSheetViewBinder::bind);

        mMediator = new FilterSheetMediator(listItems, filterItemList, this::closeSheet);
        mItemCount = listItems.size();
    }

    /** Updates the items displayed in the filter sheet. */
    public void updateItems(List<FilterItem> filterItemList) {
        mMediator.updateItems(filterItemList);
        mItemCount = filterItemList.size();
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        if (!mBottomSheetController.isSheetOpen()) return;
        if (mBaseViewHeight != mBaseView.getHeight()) {
            mBaseViewHeight = mBaseView.getHeight();
            updateSheetHeight();
        }
    }

    /**
     * Open filter bottom sheet.
     *
     * @param currentItem Initial item to be selected at the beginning. If {@code null}, no item
     *     will be selected.
     */
    public void openSheet(@Nullable FilterItem currentItem) {
        updateSheetHeight();
        mMediator.resetState(currentItem);
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }

    /**
     * Update the sheet height. Called before opening it for the first time, or while it is open in
     * order to adjust the height if the base view layout change occurs.
     */
    private void updateSheetHeight() {
        ViewGroup.LayoutParams layoutParams = mItemListView.getLayoutParams();
        if (layoutParams == null) {
            layoutParams = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0);
        }

        int rowHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size);
        layoutParams.height = calculateSheetHeight(rowHeight, mBaseView.getHeight(), mItemCount);
        mItemListView.setLayoutParams(layoutParams);
    }

    @VisibleForTesting
    static int calculateSheetHeight(int rowHeight, int baseViewHeight, int rowCount) {
        int maxHeight = (int) (baseViewHeight * MAX_SHEET_HEIGHT_RATIO);
        int visibleRowCount = Math.min(rowCount, MAX_VISIBLE_ITEM_COUNT);
        return Math.min(visibleRowCount * rowHeight, maxHeight);
    }

    private void closeSheet(@Nullable FilterItem filterItem) {
        mBottomSheetController.hideContent(mSheetContent, true);
        mCloseCallback.onFilterItemUpdated(filterItem);
    }

    private void destroy() {
        mBaseView.removeOnLayoutChangeListener(this);
    }

    void clickItemForTesting(String id) {
        mMediator.clickItemForTesting(id); // IN-TEST
    }

    void clickCloseButtonForTesting() {
        mCloseButtonModel.get(FilterSheetProperties.CLOSE_BUTTON_CALLBACK).onClick(null); // IN-TEST
    }

    @Nullable String getCurrentItemIdForTesting() {
        return mMediator.getCurrentItemIdForTesting(); // IN-TEST
    }
}
