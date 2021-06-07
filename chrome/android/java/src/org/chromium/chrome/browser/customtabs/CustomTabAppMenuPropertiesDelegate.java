// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * App menu properties delegate for {@link CustomTabActivity}.
 */
public class CustomTabAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    private static final String CUSTOM_MENU_ITEM_ID_KEY = "CustomMenuItemId";

    private final Verifier mVerifier;
    private final @CustomTabsUiType int mUiType;
    private final boolean mShowShare;
    private final boolean mShowStar;
    private final boolean mShowDownload;
    private final boolean mIsOpenedByChrome;
    private final boolean mIsIncognito;
    private final boolean mShowOpenInChrome;

    private final List<String> mMenuEntries;
    private final Map<MenuItem, Integer> mItemToIndexMap = new HashMap<MenuItem, Integer>();

    private boolean mIsCustomEntryAdded;

    /**
     * Creates an {@link CustomTabAppMenuPropertiesDelegate} instance.
     *
     * @param showOpenInChrome Whether 'open in chrome' is shown, depending upon other state.
     */
    public CustomTabAppMenuPropertiesDelegate(Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector, ToolbarManager toolbarManager, View decorView,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier, Verifier verifier,
            @CustomTabsUiType final int uiType, List<String> menuEntries, boolean isOpenedByChrome,
            boolean showShare, boolean showStar, boolean showDownload, boolean isIncognito,
            boolean showOpenInChrome) {
        super(context, activityTabProvider, multiWindowModeStateDispatcher, tabModelSelector,
                toolbarManager, decorView, null, bookmarkBridgeSupplier);
        mVerifier = verifier;
        mUiType = uiType;
        mMenuEntries = menuEntries;
        mIsOpenedByChrome = isOpenedByChrome;
        mShowShare = showShare;
        mShowStar = showStar;
        mShowDownload = showDownload;
        mIsIncognito = isIncognito;
        mShowOpenInChrome = showOpenInChrome;
    }

    @Override
    public int getAppMenuLayoutId() {
        return R.menu.custom_tabs_menu;
    }

    @Override
    public @Nullable List<CustomViewBinder> getCustomViewBinders() {
        return Collections.EMPTY_LIST;
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        Tab currentTab = mActivityTabProvider.get();
        if (currentTab != null) {
            GURL url = currentTab.getUrl();

            MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(currentTab.canGoForward());

            mReloadMenuItem = menu.findItem(R.id.reload_menu_id);
            Drawable icon = AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
            DrawableCompat.setTintList(icon,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_tint_list));
            mReloadMenuItem.setIcon(icon);
            loadingStateChanged(currentTab.isLoading());

            MenuItem shareItem = menu.findItem(R.id.share_row_menu_id);
            shareItem.setVisible(mShowShare);
            shareItem.setEnabled(mShowShare);
            if (mShowShare) {
                ShareHelper.configureDirectShareMenuItem(
                        mContext, menu.findItem(R.id.direct_share_menu_id));
            }

            boolean openInChromeItemVisible = true;
            boolean bookmarkItemVisible = mShowStar;
            boolean downloadItemVisible = mShowDownload;
            boolean addToHomeScreenVisible = true;
            boolean requestDesktopSiteVisible = true;

            if (mUiType == CustomTabsUiType.MEDIA_VIEWER) {
                // Most of the menu items don't make sense when viewing media.
                menu.findItem(R.id.icon_row_menu_id).setVisible(false);
                menu.findItem(R.id.find_in_page_id).setVisible(false);
                bookmarkItemVisible = false; // Set to skip initialization.
                downloadItemVisible = false; // Set to skip initialization.
                openInChromeItemVisible = false;
                requestDesktopSiteVisible = false;
                addToHomeScreenVisible = false;
            } else if (mUiType == CustomTabsUiType.READER_MODE) {
                // Only 'find in page' and the reader mode preference are shown for Reader Mode UI.
                menu.findItem(R.id.icon_row_menu_id).setVisible(false);
                bookmarkItemVisible = false; // Set to skip initialization.
                downloadItemVisible = false; // Set to skip initialization.
                openInChromeItemVisible = false;
                requestDesktopSiteVisible = false;
                addToHomeScreenVisible = false;

                menu.findItem(R.id.reader_mode_prefs_id).setVisible(true);
            } else if (mUiType == CustomTabsUiType.MINIMAL_UI_WEBAPP) {
                requestDesktopSiteVisible = false;
                // For Webapps & WebAPKs Verifier#wasPreviouslyVerified() performs verification
                // (instead of looking up cached value).
                addToHomeScreenVisible = !mVerifier.wasPreviouslyVerified(url.getSpec());
                downloadItemVisible = false;
                bookmarkItemVisible = false;
            } else if (mUiType == CustomTabsUiType.OFFLINE_PAGE) {
                openInChromeItemVisible = false;
                bookmarkItemVisible = true;
                downloadItemVisible = false;
                addToHomeScreenVisible = false;
                requestDesktopSiteVisible = true;
            }

            if (!FirstRunStatus.getFirstRunFlowComplete()) {
                openInChromeItemVisible = false;
                bookmarkItemVisible = false;
                downloadItemVisible = false;
                addToHomeScreenVisible = false;
            }

            if (mIsIncognito) {
                addToHomeScreenVisible = false;
                downloadItemVisible = false;
                openInChromeItemVisible = false;
            }

            boolean isChromeScheme = url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                    || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME);
            if (isChromeScheme || url.isEmpty()) {
                addToHomeScreenVisible = false;
            }

            MenuItem downloadItem = menu.findItem(R.id.offline_page_id);
            if (downloadItemVisible) {
                downloadItem.setEnabled(shouldEnableDownloadPage(currentTab));
            } else {
                downloadItem.setVisible(false);
            }

            MenuItem bookmarkItem = menu.findItem(R.id.bookmark_this_page_id);
            if (bookmarkItemVisible) {
                updateBookmarkMenuItem(bookmarkItem, currentTab);
            } else {
                bookmarkItem.setVisible(false);
            }

            prepareTranslateMenuItem(menu, currentTab);

            if (!mShowOpenInChrome) {
                openInChromeItemVisible = false;
            }
            MenuItem openInChromeItem = menu.findItem(R.id.open_in_browser_id);
            if (openInChromeItemVisible) {
                String title = mIsIncognito ?
                        ContextUtils.getApplicationContext()
                                .getString(R.string.menu_open_in_incognito_chrome) :
                        DefaultBrowserInfo.getTitleOpenInDefaultBrowser(mIsOpenedByChrome);

                openInChromeItem.setTitle(title);
            } else {
                openInChromeItem.setVisible(false);
            }

            // Add custom menu items. Make sure they are only added once.
            if (!mIsCustomEntryAdded) {
                mIsCustomEntryAdded = true;
                for (int i = 0; i < mMenuEntries.size(); i++) {
                    MenuItem item = menu.add(0, 0, 1, mMenuEntries.get(i));
                    mItemToIndexMap.put(item, i);
                }
            }

            updateRequestDesktopSiteMenuItem(menu, currentTab, requestDesktopSiteVisible);
            prepareAddToHomescreenMenuItem(menu, currentTab, addToHomeScreenVisible);
        }
    }

    /**
     * @return The index that the given menu item should appear in the result of
     *         {@link BrowserServicesIntentDataProvider#getMenuTitles()}. Returns -1 if item not
     * found.
     */
    public static int getIndexOfMenuItemFromBundle(Bundle menuItemData) {
        if (menuItemData != null && menuItemData.containsKey(CUSTOM_MENU_ITEM_ID_KEY)) {
            return menuItemData.getInt(CUSTOM_MENU_ITEM_ID_KEY);
        }

        return -1;
    }

    @Override
    public Bundle getBundleForMenuItem(MenuItem item) {
        Bundle itemBundle = super.getBundleForMenuItem(item);

        if (!mItemToIndexMap.containsKey(item)) {
            return itemBundle;
        }

        itemBundle.putInt(CUSTOM_MENU_ITEM_ID_KEY, mItemToIndexMap.get(item).intValue());
        return itemBundle;
    }

    @Override
    public int getFooterResourceId() {
        // Avoid showing the branded menu footer for media and offline pages.
        if (mUiType == CustomTabsUiType.MEDIA_VIEWER || mUiType == CustomTabsUiType.OFFLINE_PAGE) {
            return 0;
        }
        return R.layout.powered_by_chrome_footer;
    }

    /**
     * Get the {@link MenuItem} object associated with the given title. If multiple menu items have
     * the same title, a random one will be returned. This method is for testing purpose _only_.
     */
    @VisibleForTesting
    MenuItem getMenuItemForTitle(String title) {
        for (MenuItem item : mItemToIndexMap.keySet()) {
            if (item.getTitle().equals(title)) return item;
        }
        return null;
    }
}
