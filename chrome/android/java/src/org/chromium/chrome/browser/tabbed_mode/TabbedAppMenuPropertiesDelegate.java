// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.view.View;

import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.appmenu.AppMenuIconRowFooter;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.datareduction.DataReductionMainMenuItem;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;

/**
 * An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity.
 */
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    AppMenuDelegate mAppMenuDelegate;

    public TabbedAppMenuPropertiesDelegate(Context context, ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector, ToolbarManager toolbarManager, View decorView,
            AppMenuDelegate appMenuDelegate,
            ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier) {
        super(context, activityTabProvider, multiWindowModeStateDispatcher, tabModelSelector,
                toolbarManager, decorView, overviewModeBehaviorSupplier, bookmarkBridgeSupplier);
        mAppMenuDelegate = appMenuDelegate;
    }

    private boolean isMenuButtonInBottomToolbar() {
        return mToolbarManager != null && mToolbarManager.isBottomToolbarVisible();
    }

    private boolean shouldShowDataSaverMenuItem() {
        return (mOverviewModeBehavior == null || !mOverviewModeBehavior.overviewVisible())
                && DataReductionProxySettings.getInstance().shouldUseDataReductionMainMenuItem();
    }

    @Override
    public int getFooterResourceId() {
        if (isMenuButtonInBottomToolbar()) {
            return this.shouldShowPageMenu() ? R.layout.icon_row_menu_footer : 0;
        }
        return shouldShowDataSaverMenuItem() ? R.layout.data_reduction_main_menu_item : 0;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (view instanceof AppMenuIconRowFooter) {
            ((AppMenuIconRowFooter) view)
                    .initialize(appMenuHandler, mBookmarkBridge, mActivityTabProvider.get(),
                            mAppMenuDelegate);
        }
    }

    @Override
    public int getHeaderResourceId() {
        if (isMenuButtonInBottomToolbar()) {
            return shouldShowDataSaverMenuItem() ? R.layout.data_reduction_main_menu_item : 0;
        }
        return 0;
    }

    @Override
    public void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (view instanceof DataReductionMainMenuItem) {
            view.findViewById(R.id.data_reduction_menu_divider).setVisibility(View.GONE);
        }
    }

    @Override
    public boolean shouldShowFooter(int maxMenuHeight) {
        if (isMenuButtonInBottomToolbar()) return true;
        if (shouldShowDataSaverMenuItem()) {
            return canShowDataReductionItem(maxMenuHeight);
        }
        return super.shouldShowFooter(maxMenuHeight);
    }

    @Override
    public boolean shouldShowHeader(int maxMenuHeight) {
        if (!isMenuButtonInBottomToolbar()) {
            return super.shouldShowHeader(maxMenuHeight);
        }

        if (DataReductionProxySettings.getInstance().shouldUseDataReductionMainMenuItem()) {
            return canShowDataReductionItem(maxMenuHeight);
        }

        return super.shouldShowHeader(maxMenuHeight);
    }

    private boolean canShowDataReductionItem(int maxMenuHeight) {
        // TODO(twellington): Account for whether a different footer or header is
        // showing.
        return maxMenuHeight >= mContext.getResources().getDimension(
                       R.dimen.data_saver_menu_footer_min_show_height);
    }
}
