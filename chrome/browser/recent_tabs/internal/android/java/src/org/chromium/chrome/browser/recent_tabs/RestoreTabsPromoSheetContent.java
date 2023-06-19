// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.UNINITIALIZED;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREBackPressType;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFRERestoredTabsResult;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREResultAction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The bottom sheet content for the Restore Tabs promo.
 */
public class RestoreTabsPromoSheetContent implements BottomSheetContent {
    private final View mContentView;
    private final PropertyModel mModel;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetOpenedObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();

    public RestoreTabsPromoSheetContent(
            View contentView, PropertyModel model, BottomSheetController bottomSheetController) {
        mContentView = contentView;
        mModel = model;
        mBottomSheetController = bottomSheetController;

        mBottomSheetOpenedObserver = new EmptyBottomSheetObserver() {
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

    @Override
    public int getVerticalScrollOffset() {
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
                assert currentScreen == UNINITIALIZED : "Backpressing on an unidentified screen.";
        }

        if (currentScreen != UNINITIALIZED) {
            RestoreTabsMetricsHelper.recordBackPressTypeMetrics(
                    RestoreTabsOnFREBackPressType.SYSTEM_BACKPRESS);
        }
    }
}
