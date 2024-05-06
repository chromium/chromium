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

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Coordinator class of the app filter bottom sheet UI for history page. */
class AppFilterCoordinator implements View.OnLayoutChangeListener {
    // Maximum number of app filter items shown on the sheet at once if screen dimension allows.
    static final int MAX_VISIBLE_ITEM_COUNT = 5;

    // Maximum ratio of the sheet height against the base view height.
    static final float MAX_SHEET_HEIGHT_RATIO = 0.7f;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final AppFilterMediator mMediator;
    private final RecyclerView mItemListView;
    private final BottomSheetContent mSheetContent;
    private final PropertyModel mCloseButtonModel;

    private final View mContentView;
    private final View mBaseView;

    private final CloseCallback mCloseCallback;
    private final int mAppCount;

    private int mBaseViewHeight;

    /** Data class for individual app item in the filter list. */
    public static class AppInfo {
        public final String id;
        public final Drawable icon;
        public final CharSequence label;

        public AppInfo(String id, Drawable icon, CharSequence label) {
            this.id = id;
            this.icon = icon;
            this.label = label;
        }

        /** Return whether the app info object is valid. */
        public boolean isValid() {
            return id != null;
        }

        @Override
        public boolean equals(Object o) {
            if (o == this) return true;
            return (o instanceof AppInfo appInfo) ? TextUtils.equals(id, appInfo.id) : false;
        }
    }

    /** Callback to be invoked when the sheet gets closed with updated app info. */
    public interface CloseCallback {
        /**
         * @param appInfo {@link AppInfo} containing the app information. May be {@code null} if no
         *     app is selected.
         */
        void onAppUpdated(AppInfo appInfo);
    }

    /**
     * Constructor.
     *
     * @param context {@link Context} for resources, views.
     * @param baseView Base view on which the sheet is opened.
     * @param bottomSheetController {@link BottomSheetController} to open/close the sheet.
     * @param closeCallback Callback invoked when the sheet is closed
     * @param appInfoList List of the apps to display in the sheet.
     */
    AppFilterCoordinator(
            Context context,
            View baseView,
            BottomSheetController bottomSheetController,
            CloseCallback closeCallback,
            List<AppInfo> appInfoList) {
        mContext = context;
        mBaseView = baseView;
        mBaseViewHeight = mBaseView.getHeight();
        mBottomSheetController = bottomSheetController;
        mCloseCallback = closeCallback;
        var layoutInflater = LayoutInflater.from(context);
        mContentView = layoutInflater.inflate(R.layout.appfilter_content, null);
        mItemListView = (RecyclerView) mContentView.findViewById(R.id.appfilter_item_list);
        mSheetContent =
                new AppFilterSheetContent(context, mContentView, mItemListView, this::destroy);

        ModelList listItems = new ModelList();
        var adapter = new SimpleRecyclerViewAdapter(listItems);
        adapter.registerType(
                0,
                (parent) ->
                        layoutInflater.inflate(
                                R.layout.modern_list_item_small_icon_view, parent, false),
                AppFilterViewBinder::bind);
        mItemListView.setAdapter(adapter);

        // Close button at the bottom.
        View closeButton = mContentView.findViewById(R.id.close_button);
        mCloseButtonModel =
                new PropertyModel.Builder(AppFilterProperties.CLOSE_BUTTON_KEY)
                        .with(
                                AppFilterProperties.CLOSE_BUTTON_CALLBACK,
                                v -> mBottomSheetController.hideContent(mSheetContent, true))
                        .build();
        PropertyModelChangeProcessor.create(
                mCloseButtonModel, closeButton, AppFilterViewBinder::bind);

        mMediator = new AppFilterMediator(context, listItems, appInfoList, this::closeSheet);
        mAppCount = listItems.size();
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
     * Open app filter bottom sheet.
     *
     * @param currentApp Initial app to be selected at the beginning. If {@code null}, no app will
     *     be selected.
     */
    public void openSheet(@Nullable AppInfo currentApp) {
        updateSheetHeight();
        mMediator.resetState(currentApp);
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
        layoutParams.height = calculateSheetHeight(rowHeight, mBaseView.getHeight(), mAppCount);
        mItemListView.setLayoutParams(layoutParams);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static int calculateSheetHeight(int rowHeight, int baseViewHeight, int rowCount) {
        int maxHeight = (int) (baseViewHeight * MAX_SHEET_HEIGHT_RATIO);
        int visibleRowCount = Math.min(rowCount, MAX_VISIBLE_ITEM_COUNT);
        return Math.min(visibleRowCount * rowHeight, maxHeight);
    }

    private void closeSheet(AppInfo appInfo) {
        mBottomSheetController.hideContent(mSheetContent, true);
        mCloseCallback.onAppUpdated(appInfo);
    }

    private void destroy() {
        mBaseView.removeOnLayoutChangeListener(this);
    }

    void clickItemForTesting(String appId) {
        mMediator.clickItemForTesting(appId); // IN-TEST
    }

    void clickCloseButtonForTesting() {
        mCloseButtonModel.get(AppFilterProperties.CLOSE_BUTTON_CALLBACK).onClick(null); // IN-TEST
    }

    void setCurrentAppForTesting(String appId) {
        mMediator.setCurrentAppForTesting(appId); // IN-TEST
    }

    String getCurrentAppIdForTesting() {
        return mMediator.getCurrentAppIdForTesting(); // IN-TEST
    }
}
