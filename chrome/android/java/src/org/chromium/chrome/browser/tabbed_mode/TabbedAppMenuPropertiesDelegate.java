// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.datareduction.DataReductionMainMenuItem;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedMainMenuItem;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity.
 */
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    AppMenuDelegate mAppMenuDelegate;
    WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    ModalDialogManager mModalDialogManager;
    SnackbarManager mSnackbarManager;

    public TabbedAppMenuPropertiesDelegate(Context context, ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector, ToolbarManager toolbarManager, View decorView,
            AppMenuDelegate appMenuDelegate,
            OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier,
            WebFeedSnackbarController.FeedLauncher feedLauncher,
            ModalDialogManager modalDialogManager, SnackbarManager snackbarManager) {
        super(context, activityTabProvider, multiWindowModeStateDispatcher, tabModelSelector,
                toolbarManager, decorView, overviewModeBehaviorSupplier, bookmarkBridgeSupplier);
        mAppMenuDelegate = appMenuDelegate;
        mFeedLauncher = feedLauncher;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;
    }

    private boolean shouldShowDataSaverMenuItem() {
        return (mOverviewModeBehavior == null || !mOverviewModeBehavior.overviewVisible())
                && DataReductionProxySettings.getInstance().shouldUseDataReductionMainMenuItem();
    }

    private boolean shouldShowWebFeedMenuItem() {
        if (!FeedFeatures.isWebFeedUIEnabled()) {
            return false;
        }
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isIncognito() || OfflinePageUtils.isOfflinePage(tab)) {
            return false;
        }
        String url = tab.getOriginalUrl().getSpec();
        return url.startsWith(UrlConstants.HTTP_URL_PREFIX)
                || url.startsWith(UrlConstants.HTTPS_URL_PREFIX);
    }

    @Override
    public int getFooterResourceId() {
        if (shouldShowWebFeedMenuItem()) {
            return R.layout.web_feed_main_menu_item;
        } else if (shouldShowDataSaverMenuItem()) {
            return R.layout.data_reduction_main_menu_item;
        }
        return 0;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (view instanceof WebFeedMainMenuItem) {
            ((WebFeedMainMenuItem) view)
                    .initialize(mActivityTabProvider.get(), appMenuHandler,
                            new LargeIconBridge(Profile.getLastUsedRegularProfile()), mFeedLauncher,
                            mModalDialogManager, mSnackbarManager);
        }
    }

    @Override
    public int getHeaderResourceId() {
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
        if (shouldShowWebFeedMenuItem()) {
            return true;
        }
        if (shouldShowDataSaverMenuItem()) {
            return canShowDataReductionItem(maxMenuHeight);
        }
        return super.shouldShowFooter(maxMenuHeight);
    }

    @Override
    protected boolean shouldShowManagedByMenuItem(Tab currentTab) {
        return ManagedBrowserUtils.hasBrowserPoliciesApplied(
                Profile.fromWebContents(currentTab.getWebContents()));
    }

    @Override
    public boolean shouldShowIconBeforeItem() {
        return true;
    }

    private boolean canShowDataReductionItem(int maxMenuHeight) {
        // TODO(twellington): Account for whether a different footer or header is
        // showing.
        return maxMenuHeight >= mContext.getResources().getDimension(
                       R.dimen.data_saver_menu_footer_min_show_height);
    }
}
