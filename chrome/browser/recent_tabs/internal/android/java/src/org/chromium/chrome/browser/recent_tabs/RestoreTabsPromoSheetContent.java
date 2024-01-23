// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.UNINITIALIZED;

import android.view.View;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREBackPressType;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFRERestoredTabsResult;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREResultAction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** The bottom sheet content for the Restore Tabs promo. */
public class RestoreTabsPromoSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final PropertyModel mModel;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetOpenedObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();
    private ScrollView mScrollView;
    private RecyclerView mRecyclerView;

    public RestoreTabsPromoSheetContent(
            View contentView, PropertyModel model, BottomSheetController bottomSheetController) {
        mContentView = contentView;
        mModel = model;
        mBottomSheetController = bottomSheetController;
        mScrollView = mContentView.findViewById(R.id.restore_tabs_promo_sheet_scrollview);
        mRecyclerView = mContentView.findViewById(R.id.restore_tabs_detail_screen_recycler_view);

        mBottomSheetOpenedObserver =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetOpened(@BottomSheetController.StateChangeReason int reason) {
                        super.onSheetOpened(reason);
                        mBackPressStateChangedSupplier.set(true);
                    }

                    @Override
                    public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                        super.onSheetClosed(reason);
                        mBackPressStateChangedSupplier.set(false);
                        mBottomSheetController.removeObserver(mBottomSheetOpenedObserver);
                    }
                };
        mBottomSheetController.addObserver(mBottomSheetOpenedObserver);
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

    /**
     * The vertical scroll offset of the recycler view's list containing the user's devices
     * or the currently selected device's tab items. The offset prevents scroll flinging from
     * dismissing the sheet.
     */
    @Override
    public int getVerticalScrollOffset() {
        int currentScreen = mModel.get(RestoreTabsProperties.CURRENT_SCREEN);

        if (currentScreen == HOME_SCREEN) {
            if (mScrollView != null) {
                // Calculate the scroll position of the scrollview and make sure it is
                // non-zero, otherwise allows swipe to dismiss on the bottom sheet.
                return mScrollView.getScrollY();
            }
        } else if (currentScreen == DEVICE_SCREEN || currentScreen == REVIEW_TABS_SCREEN) {
            // Get the first item in the recycler view to make sure it is scrolled off-screen
            // (has non-zero value) otherwise allow swipe to dismiss on the bottom sheet.
            View v = mRecyclerView.getChildAt(0);
            return v == null ? 0 : -(v.getTop() - mRecyclerView.getPaddingTop());
        }

        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean handleBackPress() {
        backPressOnCurrentScreen();
        return mModel.get(RestoreTabsProperties.CURRENT_SCREEN) != UNINITIALIZED;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        backPressOnCurrentScreen();
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.restore_tabs_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.restore_tabs_sheet_closed;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.restore_tabs_content_description;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.restore_tabs_content_description;
    }

    private void backPressOnCurrentScreen() {
        int currentScreen = mModel.get(RestoreTabsProperties.CURRENT_SCREEN);

        switch (currentScreen) {
            case DEVICE_SCREEN:
                mModel.set(RestoreTabsProperties.CURRENT_SCREEN, HOME_SCREEN);
                break;
            case REVIEW_TABS_SCREEN:
                mModel.set(RestoreTabsProperties.CURRENT_SCREEN, HOME_SCREEN);
                break;
            case HOME_SCREEN:
                mModel.set(RestoreTabsProperties.VISIBLE, false);
                RestoreTabsMetricsHelper.recordResultActionHistogram(
                        RestoreTabsOnFREResultAction.DISMISSED_BACKPRESS);
                RestoreTabsMetricsHelper.recordResultActionMetrics(
                        RestoreTabsOnFREResultAction.DISMISSED_BACKPRESS);
                RestoreTabsMetricsHelper.recordRestoredTabsResultHistogram(
                        RestoreTabsOnFRERestoredTabsResult.NONE);
                break;
            default:
                assert currentScreen == UNINITIALIZED : "Back pressing on an unidentified screen.";
        }

        if (currentScreen != UNINITIALIZED) {
            RestoreTabsMetricsHelper.recordBackPressTypeMetrics(
                    RestoreTabsOnFREBackPressType.SYSTEM_BACKPRESS);
        }
    }

    void setRecyclerViewForTesting(RecyclerView recyclerView) {
        mRecyclerView = recyclerView;
    }

    void setScrollViewForTesting(ScrollView scrollView) {
        mScrollView = scrollView;
    }
}
