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

import androidx.annotation.IdRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.url.GURL;

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
    private final Map<Integer, Integer> mItemIdToIndexMap = new HashMap<Integer, Integer>();
    private final Supplier<ContextualPageActionController> mContextualPageActionControllerSupplier;

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
            Supplier<ContextualPageActionController> contextualPageActionControllerSupplier,
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
        mContextualPageActionControllerSupplier = contextualPageActionControllerSupplier;
        mHasClientPackage = hasClientPackage;
    }

    @Override
    public int getAppMenuLayoutId() {
        return R.menu.custom_tabs_menu;
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        Tab currentTab = mActivityTabProvider.get();
        if (currentTab != null) {
            GURL url = currentTab.getUrl();

            boolean iconRowVisible = true;
            boolean findInPageVisible = true;
            boolean openInChromeItemVisible = true;
            boolean bookmarkItemVisible = mShowStar;
            boolean downloadItemVisible = mShowDownload;
            boolean addToHomeScreenVisible = true;
            boolean requestDesktopSiteVisible = true;
            boolean tryAddingReadAloud = ReadAloudFeatures.isEnabledForOverflowMenuInCct();
            boolean readerModePrefsVisible = false;

            if (mUiType == CustomTabsUiType.MEDIA_VIEWER) {
                // Most of the menu items don't make sense when viewing media.
                iconRowVisible = false;
                findInPageVisible = false;
                bookmarkItemVisible = false; // Set to skip initialization.
                downloadItemVisible = false; // Set to skip initialization.
                openInChromeItemVisible = false;
                requestDesktopSiteVisible = false;
                addToHomeScreenVisible = false;
                tryAddingReadAloud = false;
            } else if (mUiType == CustomTabsUiType.READER_MODE) {
                // Only 'find in page' and the reader mode preference are shown for Reader Mode UI.
                iconRowVisible = false;
                bookmarkItemVisible = false; // Set to skip initialization.
                downloadItemVisible = false; // Set to skip initialization.
                openInChromeItemVisible = false;
                requestDesktopSiteVisible = false;
                addToHomeScreenVisible = false;
                tryAddingReadAloud = false;
                readerModePrefsVisible = true;
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
            } else if (mUiType == CustomTabsUiType.NETWORK_BOUND_TAB) {
                openInChromeItemVisible = false;
                addToHomeScreenVisible = false;
                requestDesktopSiteVisible = true;
            } else if (mUiType == CustomTabsUiType.POPUP) {
                openInChromeItemVisible = false;
                bookmarkItemVisible = false;
                downloadItemVisible = false;
                addToHomeScreenVisible = false;
                requestDesktopSiteVisible = false;
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
            // TODO(crbug.com/384992232): Hide open in Chrome for blob and data url until such view
            //  intent can be handled.
            if ((ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_OPEN_PDF_INLINE)
                            || ChromeFeatureList.isEnabled(
                                    ChromeFeatureList.ANDROID_OPEN_PDF_INLINE_BACKPORT))
                    && (url.getScheme().equals(UrlConstants.BLOB_SCHEME)
                            || url.getScheme().equals(UrlConstants.DATA_SCHEME))) {
                openInChromeItemVisible = false;
            }
            if (isNativePage || isFileScheme || isContentScheme || url.isEmpty()) {
                addToHomeScreenVisible = false;
            }

            if (!WebappsUtils.isAddToHomeIntentSupported()) {
                addToHomeScreenVisible = false;
            }

            // --- Icon Row ---
            MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(currentTab.canGoForward());

            Drawable icon = AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
            DrawableCompat.setTintList(
                    icon,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_tint_list));
            menu.findItem(R.id.reload_menu_id).setIcon(icon);
            loadingStateChanged(currentTab.isLoading());

            MenuItem downloadItem = menu.findItem(R.id.offline_page_id);
            if (downloadItemVisible) {
                downloadItem.setEnabled(shouldEnableDownloadPage(currentTab));
            } else {
                downloadItem.setVisible(false);
            }

            MenuItem bookmarkItem = menu.findItem(R.id.bookmark_this_page_id);
            if (bookmarkItemVisible) {
                updateBookmarkMenuItemShortcut(bookmarkItem, currentTab, /* fromCct= */ true);
            } else {
                bookmarkItem.setVisible(false);
            }

            menu.findItem(R.id.icon_row_menu_id).setVisible(iconRowVisible);

            // --- App Specific Items / Divider ---
            for (int i = 0; i < mMenuEntries.size(); i++) {
                @IdRes
                int id =
                        switch (i) {
                            case 0 -> R.id.custom_tabs_app_menu_item_id_0;
                            case 1 -> R.id.custom_tabs_app_menu_item_id_1;
                            case 2 -> R.id.custom_tabs_app_menu_item_id_2;
                            case 3 -> R.id.custom_tabs_app_menu_item_id_3;
                            case 4 -> R.id.custom_tabs_app_menu_item_id_4;
                            case 5 -> R.id.custom_tabs_app_menu_item_id_5;
                            case 6 -> R.id.custom_tabs_app_menu_item_id_6;
                            default -> {
                                assert false : "Only 7 custom menu items are currently allowed.";
                                yield 0;
                            }
                        };
                MenuItem item = menu.add(0, id, 1, mMenuEntries.get(i));
                mItemIdToIndexMap.put(item.getItemId(), i);
            }

            if (mMenuEntries.size() == 0) {
                menu.removeItem(R.id.divider_line_id);
            }

            // --- Read Aloud ---
            if (tryAddingReadAloud) {
                // Set visibility of Read Aloud menu item. The entrypoint will be
                // visible iff the tab can be synthesized.
                prepareReadAloudMenuItem(menu, currentTab);
            } else {
                menu.findItem(R.id.readaloud_menu_id).setVisible(false);
            }

            // --- Share ---
            MenuItem shareItem = menu.findItem(R.id.share_row_menu_id);
            shareItem.setVisible(mShowShare);
            shareItem.setEnabled(mShowShare);
            if (mShowShare) {
                updateDirectShareMenuItem(menu.findItem(R.id.direct_share_menu_id));
            }

            // --- History ---
            if (!CustomTabAppMenuHelper.showHistoryItem(mHasClientPackage, mUiType)) {
                menu.findItem(R.id.open_history_menu_id).setVisible(false);
            }

            // --- Find in Page ---
            menu.findItem(R.id.find_in_page_id).setVisible(findInPageVisible);

            // --- Reader Mode Prefs ---
            menu.findItem(R.id.reader_mode_prefs_id).setVisible(readerModePrefsVisible);

            // --- Price Tracking / Price Insights ---
            MenuItem startPriceTrackingMenuItem = menu.findItem(R.id.enable_price_tracking_menu_id);
            MenuItem stopPriceTrackingMenuItem = menu.findItem(R.id.disable_price_tracking_menu_id);
            startPriceTrackingMenuItem.setVisible(false);
            stopPriceTrackingMenuItem.setVisible(false);
            if (ChromeFeatureList.sCctAdaptiveButton.isEnabled()) {
                // TODO(crbug.com/391931899): Also check the dev-controlled flag
                updatePriceTrackingMenuItemRow(
                        startPriceTrackingMenuItem, stopPriceTrackingMenuItem, currentTab);
                var cpaController = mContextualPageActionControllerSupplier.get();
                if (cpaController != null) {
                    menu.findItem(R.id.price_insights_menu_id)
                            .setVisible(cpaController.hasPriceInsights());
                }
            }

            // --- Add to Homescreen / Open WebAPK ---
            prepareAddToHomescreenMenuItem(menu, currentTab, addToHomeScreenVisible);

            // --- Request Desktop Site ---
            updateRequestDesktopSiteMenuItem(
                    menu, currentTab, requestDesktopSiteVisible, isNativePage);

            // --- Translate ---
            prepareTranslateMenuItem(menu, currentTab);

            // --- Open with ---
            boolean showOpenWith = currentTab.isNativePage() && currentTab.getNativePage().isPdf();
            menu.findItem(R.id.open_with_id).setVisible(showOpenWith);

            // --- Open in browser ---
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

    @Override
    public boolean isMenuIconAtStart() {
        return mIsStartIconMenu;
    }

    void setHasClientPackageForTesting(boolean hasClientPackage) {
        mHasClientPackage = hasClientPackage;
    }
}
