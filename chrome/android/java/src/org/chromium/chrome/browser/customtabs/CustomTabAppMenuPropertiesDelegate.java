// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.app.appmenu.DividerLineMenuItemViewBinder;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** App menu properties delegate for {@link CustomTabActivity}. */
public class CustomTabAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    private static final String CUSTOM_MENU_ITEM_ID_KEY = "CustomMenuItemId";

    private final Verifier mVerifier;
    private final @CustomTabsUiType int mUiType;
    private final boolean mShowShare;
    private final boolean mShowStar;
    private final boolean mShowDownload;
    private final boolean mIsOpenedByChrome;
    private final boolean mIsIncognitoBranded;
    private final boolean mIsOffTheRecord;
    private final boolean mIsStartIconMenu;

    private final List<String> mMenuEntries;
    private final Map<String, Integer> mTitleToItemIdMap = new HashMap<String, Integer>();
    private final Map<Integer, Integer> mItemIdToIndexMap = new HashMap<Integer, Integer>();

    private boolean mHasClientPackage;

    /** Creates an {@link CustomTabAppMenuPropertiesDelegate} instance. */
    public CustomTabAppMenuPropertiesDelegate(
            Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager,
            View decorView,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            Verifier verifier,
            @CustomTabsUiType final int uiType,
            List<String> menuEntries,
            boolean isOpenedByChrome,
            boolean showShare,
            boolean showStar,
            boolean showDownload,
            boolean isIncognitoBranded,
            boolean isOffTheRecord,
            boolean isStartIconMenu,
            Supplier<ReadAloudController> readAloudControllerSupplier,
            boolean hasClientPackage) {
        super(
                context,
                activityTabProvider,
                multiWindowModeStateDispatcher,
                tabModelSelector,
                toolbarManager,
                decorView,
                null,
                bookmarkModelSupplier,
                null,
                readAloudControllerSupplier);
        mVerifier = verifier;
        mUiType = uiType;
        mMenuEntries = menuEntries;
        mIsOpenedByChrome = isOpenedByChrome;
        mShowShare = showShare && mUiType != CustomTabsUiType.AUTH_TAB;
        mShowStar = showStar;
        mShowDownload = showDownload;
        mIsIncognitoBranded = isIncognitoBranded;
        mIsOffTheRecord = isOffTheRecord;
        mIsStartIconMenu = isStartIconMenu;
        mHasClientPackage = hasClientPackage;
    }

    @Override
    public int getAppMenuLayoutId() {
        return R.menu.custom_tabs_menu;
    }

    @Override
    public @Nullable List<CustomViewBinder> getCustomViewBinders() {
        List<CustomViewBinder> customViewBinders = new ArrayList<>();
        customViewBinders.add(new DividerLineMenuItemViewBinder());
        return customViewBinders;
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        Tab currentTab = mActivityTabProvider.get();
        if (currentTab != null) {
            GURL url = currentTab.getUrl();

            MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(currentTab.canGoForward());

            Drawable icon = AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
            DrawableCompat.setTintList(
                    icon,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_tint_list));
            menu.findItem(R.id.reload_menu_id).setIcon(icon);
            loadingStateChanged(currentTab.isLoading());

            MenuItem shareItem = menu.findItem(R.id.share_row_menu_id);
            shareItem.setVisible(mShowShare);
            shareItem.setEnabled(mShowShare);
            if (mShowShare) {
                updateDirectShareMenuItem(menu.findItem(R.id.direct_share_menu_id));
            }

            boolean openInChromeItemVisible = true;
            boolean bookmarkItemVisible = mShowStar;
            boolean downloadItemVisible = mShowDownload;
            boolean addToHomeScreenVisible = true;
            boolean requestDesktopSiteVisible = true;
            boolean tryAddingReadAloud = ReadAloudFeatures.isEnabledForOverflowMenuInCCT();

            if (mUiType == CustomTabsUiType.MEDIA_VIEWER) {
                // Most of the menu items don't make sense when viewing media.
                menu.findItem(R.id.icon_row_menu_id).setVisible(false);
                menu.findItem(R.id.find_in_page_id).setVisible(false);
                bookmarkItemVisible = false; // Set to skip initialization.
                downloadItemVisible = false; // Set to skip initialization.
                openInChromeItemVisible = false;
                requestDesktopSiteVisible = false;
                addToHomeScreenVisible = false;
                tryAddingReadAloud = false;
            } else if (mUiType == CustomTabsUiType.READER_MODE) {
                // Only 'find in page' and the reader mode preference are shown for Reader Mode UI.
                menu.findItem(R.id.icon_row_menu_id).setVisible(false);
                bookmarkItemVisible = false; // Set to skip initialization.
                downloadItemVisible = false; // Set to skip initialization.
                openInChromeItemVisible = false;
                requestDesktopSiteVisible = false;
                addToHomeScreenVisible = false;
                tryAddingReadAloud = false;
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
                tryAddingReadAloud = false;
            } else if (mUiType == CustomTabsUiType.AUTH_TAB) {
                openInChromeItemVisible = false;
                bookmarkItemVisible = false;
                downloadItemVisible = false;
                addToHomeScreenVisible = false;
                tryAddingReadAloud = false;
            }

            if (!FirstRunStatus.getFirstRunFlowComplete()) {
                openInChromeItemVisible = false;
                bookmarkItemVisible = false;
                downloadItemVisible = false;
                addToHomeScreenVisible = false;
            }

            if (mIsIncognitoBranded) {
                addToHomeScreenVisible = false;
                downloadItemVisible = false;
                openInChromeItemVisible = false;
                tryAddingReadAloud = false;
            }

            boolean isNativePage =
                    url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                            || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)
                            || currentTab.isNativePage();
            boolean isFileScheme = url.getScheme().equals(UrlConstants.FILE_SCHEME);
            boolean isContentScheme = url.getScheme().equals(UrlConstants.CONTENT_SCHEME);
            if (isNativePage || isFileScheme || isContentScheme || url.isEmpty()) {
                addToHomeScreenVisible = false;
            }

            if (!WebappsUtils.isAddToHomeIntentSupported()) {
                addToHomeScreenVisible = false;
            }

            if (!CustomTabAppMenuHelper.showHistoryItem(mHasClientPackage, mUiType)) {
                menu.findItem(R.id.open_history_menu_id).setVisible(false);
            }

            MenuItem downloadItem = menu.findItem(R.id.offline_page_id);
            if (downloadItemVisible) {
                downloadItem.setEnabled(shouldEnableDownloadPage(currentTab));
            } else {
                downloadItem.setVisible(false);
            }

            MenuItem bookmarkItem = menu.findItem(R.id.bookmark_this_page_id);
            if (bookmarkItemVisible) {
                updateBookmarkMenuItemShortcut(bookmarkItem, currentTab, /* fromCCT= */ true);
            } else {
                bookmarkItem.setVisible(false);
            }

            prepareTranslateMenuItem(menu, currentTab);

            if (tryAddingReadAloud) {
                // Set visibility of Read Aloud menu item. The entrypoint will be
                // visible iff the tab can be synthesized.
                prepareReadAloudMenuItem(menu, currentTab);
            } else {
                menu.findItem(R.id.readaloud_menu_id).setVisible(false);
            }

            MenuItem openInChromeItem = menu.findItem(R.id.open_in_browser_id);
            if (openInChromeItemVisible) {
                String title =
                        mIsOffTheRecord
                                ? ContextUtils.getApplicationContext()
                                        .getString(R.string.menu_open_in_incognito_chrome)
                                : DefaultBrowserInfo.getTitleOpenInDefaultBrowser(
                                        mIsOpenedByChrome);

                openInChromeItem.setTitle(title);
            } else {
                openInChromeItem.setVisible(false);
            }

            // Add custom menu items.
            for (int i = 0; i < mMenuEntries.size(); i++) {
                MenuItem item = menu.add(0, i, 1, mMenuEntries.get(i));
                mTitleToItemIdMap.put(mMenuEntries.get(i), item.getItemId());
                mItemIdToIndexMap.put(item.getItemId(), i);
            }

            if (mMenuEntries.size() == 0) {
                menu.removeItem(R.id.divider_line_id);
            }

            updateRequestDesktopSiteMenuItem(
                    menu, currentTab, requestDesktopSiteVisible, isNativePage);
            prepareAddToHomescreenMenuItem(menu, currentTab, addToHomeScreenVisible);
        }
    }

    /**
     * @return The index that the given menu item should appear in the result of {@link
     *     BrowserServicesIntentDataProvider#getMenuTitles()}. Returns -1 if item not found.
     */
    public static int getIndexOfMenuItemFromBundle(Bundle menuItemData) {
        if (menuItemData != null && menuItemData.containsKey(CUSTOM_MENU_ITEM_ID_KEY)) {
            return menuItemData.getInt(CUSTOM_MENU_ITEM_ID_KEY);
        }

        return -1;
    }

    @Override
    public Bundle getBundleForMenuItem(int itemId) {
        if (!mItemIdToIndexMap.containsKey(itemId)) {
            return null;
        }

        Bundle itemBundle = new Bundle();
        itemBundle.putInt(CUSTOM_MENU_ITEM_ID_KEY, mItemIdToIndexMap.get(itemId).intValue());
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

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        super.onFooterViewInflated(appMenuHandler, view);

        TextView footerTextView = view.findViewById(R.id.running_in_chrome_footer_text);
        if (footerTextView != null) {
            String appName = view.getResources().getString(R.string.app_name);
            String footerText =
                    view.getResources().getString(R.string.twa_running_in_chrome_template, appName);
            footerTextView.setText(footerText);
        }
    }

    /**
     * Get the menu item's id object associated with the given title. If multiple menu items have
     * the same title, a random one will be returned. If the menu item is not found, return -1. This
     * method is for testing purpose _only_.
     */
    @VisibleForTesting
    int getItemIdForTitle(String title) {
        if (mTitleToItemIdMap.containsKey(title)) {
            return mTitleToItemIdMap.get(title).intValue();
        }
        return AppMenuPropertiesDelegate.INVALID_ITEM_ID;
    }

    @Override
    public boolean isMenuIconAtStart() {
        return mIsStartIconMenu;
    }

    void setHasClientPackageForTesting(boolean hasClientPackage) {
        mHasClientPackage = hasClientPackage;
    }
}
