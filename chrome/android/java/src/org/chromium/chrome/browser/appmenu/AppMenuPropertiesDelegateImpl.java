// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObservableSupplier;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.util.ArrayList;
import java.util.List;

/**
 * Base implementation of {@link AppMenuPropertiesDelegate} that handles hiding and showing menu
 * items based on activity state.
 */
public class AppMenuPropertiesDelegateImpl implements AppMenuPropertiesDelegate {
    protected MenuItem mReloadMenuItem;

    protected final Context mContext;
    protected final boolean mIsTablet;
    protected final ActivityTabProvider mActivityTabProvider;
    protected final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    protected final TabModelSelector mTabModelSelector;
    protected final ToolbarManager mToolbarManager;
    protected final View mDecorView;
    private final @Nullable ObservableSupplier<OverviewModeBehavior> mOverviewModeBehaviorSupplier;
    private final ObservableSupplier<BookmarkBridge> mBookmarkBridgeSupplier;
    private @Nullable Callback<OverviewModeBehavior> mOverviewModeSupplierCallback;
    private Callback<BookmarkBridge> mBookmarkBridgeSupplierCallback;
    private boolean mUpdateMenuItemVisible;

    protected @Nullable OverviewModeBehavior mOverviewModeBehavior;
    protected BookmarkBridge mBookmarkBridge;
    protected Runnable mAppMenuInvalidator;

    /**
     * Construct a new {@link AppMenuPropertiesDelegateImpl}.
     * @param context The activity context.
     * @param activityTabProvider The {@link ActivityTabProvider} for the containing activity.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} for the
     *         containing activity.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param toolbarManager The {@link ToolbarManager} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *         activity.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *         {@link OverviewModeBehavior} associated with the containing activity.
     * @param bookmarkBridgeSupplier An {@link ObservableSupplier} for the {@link BookmarkBridge}
     *         associated with the containing activity.
     */
    public AppMenuPropertiesDelegateImpl(Context context, ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector, ToolbarManager toolbarManager, View decorView,
            @Nullable ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier) {
        mContext = context;
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        mActivityTabProvider = activityTabProvider;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mTabModelSelector = tabModelSelector;
        mToolbarManager = toolbarManager;
        mDecorView = decorView;

        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;
        if (mOverviewModeBehaviorSupplier != null) {
            mOverviewModeSupplierCallback = overviewModeBehavior -> {
                mOverviewModeBehavior = overviewModeBehavior;
            };
            mOverviewModeBehaviorSupplier.addObserver(mOverviewModeSupplierCallback);
        }

        mBookmarkBridgeSupplier = bookmarkBridgeSupplier;
        mBookmarkBridgeSupplierCallback = (bookmarkBridge) -> mBookmarkBridge = bookmarkBridge;
        mBookmarkBridgeSupplier.addObserver(mBookmarkBridgeSupplierCallback);
    }

    @Override
    public void destroy() {
        if (mOverviewModeBehaviorSupplier != null) {
            mOverviewModeBehaviorSupplier.removeObserver(mOverviewModeSupplierCallback);
        }
        mBookmarkBridgeSupplier.removeObserver(mBookmarkBridgeSupplierCallback);
    }

    @Override
    public int getAppMenuLayoutId() {
        return R.menu.main_menu;
    }

    @Override
    public @Nullable List<CustomViewBinder> getCustomViewBinders() {
        List<CustomViewBinder> customViewBinders = new ArrayList<>();
        customViewBinders.add(new UpdateMenuItemViewBinder());
        return customViewBinders;
    }

    /**
     * @return Whether the app menu for a web page should be shown.
     */
    protected boolean shouldShowPageMenu() {
        boolean isOverview =
                mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible();

        if (mIsTablet) {
            boolean hasTabs = mTabModelSelector.getCurrentModel().getCount() != 0;
            return hasTabs && !isOverview;
        } else {
            return !isOverview && mActivityTabProvider.get() != null;
        }
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        // Exactly one of these will be true, depending on the type of menu showing.
        boolean isPageMenu = shouldShowPageMenu();
        boolean isOverviewMenu;
        boolean isTabletEmptyModeMenu;

        boolean isOverview =
                mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible();
        boolean isIncognito = mTabModelSelector.getCurrentModel().isIncognito();
        Tab currentTab = mActivityTabProvider.get();

        // Determine which menu to show.
        if (mIsTablet) {
            boolean hasTabs = mTabModelSelector.getCurrentModel().getCount() != 0;
            isOverviewMenu = hasTabs && isOverview;
            isTabletEmptyModeMenu = !hasTabs;
        } else {
            isOverviewMenu = isOverview;
            isTabletEmptyModeMenu = false;
        }
        int visibleMenus =
                (isPageMenu ? 1 : 0) + (isOverviewMenu ? 1 : 0) + (isTabletEmptyModeMenu ? 1 : 0);
        assert visibleMenus == 1;

        menu.setGroupVisible(R.id.PAGE_MENU, isPageMenu);
        menu.setGroupVisible(R.id.OVERVIEW_MODE_MENU, isOverviewMenu);
        menu.setGroupVisible(R.id.TABLET_EMPTY_MODE_MENU, isTabletEmptyModeMenu);

        if (isPageMenu && currentTab != null) {
            String url = currentTab.getUrl();
            boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                    || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
            boolean isFileScheme = url.startsWith(UrlConstants.FILE_URL_PREFIX);
            boolean isContentScheme = url.startsWith(UrlConstants.CONTENT_URL_PREFIX);
            boolean shouldShowIconRow = !mIsTablet
                    || mDecorView.getWidth()
                            < DeviceFormFactor.getNonMultiDisplayMinimumTabletWidthPx(mContext);

            final boolean bottomToolbarVisible =
                    mToolbarManager != null && mToolbarManager.isBottomToolbarVisible();
            shouldShowIconRow &= !bottomToolbarVisible;

            // Update the icon row items (shown in narrow form factors).
            menu.findItem(R.id.icon_row_menu_id).setVisible(shouldShowIconRow);
            if (shouldShowIconRow) {
                // Disable the "Forward" menu item if there is no page to go to.
                MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
                forwardMenuItem.setEnabled(currentTab.canGoForward());

                mReloadMenuItem = menu.findItem(R.id.reload_menu_id);
                Drawable icon =
                        AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
                DrawableCompat.setTintList(icon,
                        AppCompatResources.getColorStateList(mContext, R.color.standard_mode_tint));
                mReloadMenuItem.setIcon(icon);
                loadingStateChanged(currentTab.isLoading());

                MenuItem bookmarkMenuItem = menu.findItem(R.id.bookmark_this_page_id);
                updateBookmarkMenuItem(bookmarkMenuItem, currentTab);

                MenuItem offlineMenuItem = menu.findItem(R.id.offline_page_id);
                if (offlineMenuItem != null) {
                    offlineMenuItem.setEnabled(DownloadUtils.isAllowedToDownloadPage(currentTab));
                }
            }

            mUpdateMenuItemVisible =
                    UpdateMenuItemHelper.getInstance().getUiState().itemState != null;
            menu.findItem(R.id.update_menu_id).setVisible(mUpdateMenuItemVisible);
            if (mUpdateMenuItemVisible) {
                mAppMenuInvalidator = () -> handler.invalidateAppMenu();
                UpdateMenuItemHelper.getInstance().registerObserver(mAppMenuInvalidator);
            }

            boolean hasMoreThanOneTab = mTabModelSelector.getTotalTabCount() > 1;
            menu.findItem(R.id.move_to_other_window_menu_id)
                    .setVisible(mMultiWindowModeStateDispatcher.isOpenInOtherWindowSupported()
                            && hasMoreThanOneTab);

            MenuItem recentTabsMenuItem = menu.findItem(R.id.recent_tabs_menu_id);
            recentTabsMenuItem.setVisible(!isIncognito);
            recentTabsMenuItem.setTitle(R.string.menu_recent_tabs);

            MenuItem allBookmarksMenuItem = menu.findItem(R.id.all_bookmarks_menu_id);
            allBookmarksMenuItem.setTitle(mContext.getString(R.string.menu_bookmarks));

            // Don't allow either "chrome://" pages or interstitial pages to be shared.
            menu.findItem(R.id.share_row_menu_id)
                    .setVisible(!isChromeScheme && !currentTab.isShowingInterstitialPage());

            ShareHelper.configureDirectShareMenuItem(
                    mContext, menu.findItem(R.id.direct_share_menu_id));

            // Disable find in page on the native NTP.
            menu.findItem(R.id.find_in_page_id)
                    .setVisible(!currentTab.isNativePage() && currentTab.getWebContents() != null);

            // Prepare translate menu button.
            prepareTranslateMenuItem(menu, currentTab);

            // Hide 'Add to homescreen' for the following:
            // * chrome:// pages - Android doesn't know how to direct those URLs.
            // * incognito pages - To avoid problems where users create shortcuts in incognito
            //                      mode and then open the webapp in regular mode.
            // * file:// - After API 24, file: URIs are not supported in VIEW intents and thus
            //             can not be added to the homescreen.
            // * content:// - Accessing external content URIs requires the calling app to grant
            //                access to the resource via FLAG_GRANT_READ_URI_PERMISSION, and that
            //                is not persisted when adding to the homescreen.
            // * If creating shortcuts it not supported by the current home screen.
            boolean canShowHomeScreenMenuItem = ShortcutHelper.isAddToHomeIntentSupported()
                    && !isChromeScheme && !isFileScheme && !isContentScheme && !isIncognito
                    && !TextUtils.isEmpty(url);
            prepareAddToHomescreenMenuItem(menu, currentTab, canShowHomeScreenMenuItem);

            updateRequestDesktopSiteMenuItem(menu, currentTab, true /* can show */);

            // Only display reader mode settings menu option if the current page is in reader mode.
            menu.findItem(R.id.reader_mode_prefs_id)
                    .setVisible(DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl()));

            // Only display the Enter VR button if VR Shell Dev environment is enabled.
            menu.findItem(R.id.enter_vr_id)
                    .setVisible(CommandLine.getInstance().hasSwitch(
                            ChromeSwitches.ENABLE_VR_SHELL_DEV));
        }

        if (isOverviewMenu) {
            if (isIncognito) {
                // Hide normal close all tabs item.
                menu.findItem(R.id.close_all_tabs_menu_id).setVisible(false);
                // Enable close incognito tabs only if there are incognito tabs.
                menu.findItem(R.id.close_all_incognito_tabs_menu_id).setEnabled(true);
            } else {
                // Hide close incognito tabs item.
                menu.findItem(R.id.close_all_incognito_tabs_menu_id).setVisible(false);
                // Enable close all tabs if there are normal tabs or incognito tabs.
                menu.findItem(R.id.close_all_tabs_menu_id)
                        .setEnabled(mTabModelSelector.getTotalTabCount() > 0);
            }
            if (!FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()
                    || DeviceClassManager.enableAccessibilityLayout()) {
                menu.findItem(R.id.menu_group_tabs).setVisible(false);
            } else {
                boolean shouldEnabled = mTabModelSelector.getTabModelFilterProvider()
                                                .getCurrentTabModelFilter()
                                                .getTabsWithNoOtherRelatedTabs()
                                                .size()
                        > 1;
                menu.findItem(R.id.menu_group_tabs).setEnabled(shouldEnabled);
            }
        }

        // We have to iterate all menu items since same menu item ID may be associated with more
        // than one menu items.
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.new_incognito_tab_menu_id) {
                item.setTitle(R.string.menu_new_incognito_tab);
            } else if (item.getItemId() == R.id.close_all_incognito_tabs_menu_id) {
                item.setTitle(R.string.menu_close_all_incognito_tabs);
            }
        }

        // Disable new incognito tab when it is blocked (e.g. by a policy).
        // findItem(...).setEnabled(...)" is not enough here, because of the inflated
        // main_menu.xml contains multiple items with the same id in different groups
        // e.g.: new_incognito_tab_menu_id.
        disableEnableMenuItem(menu, R.id.new_incognito_tab_menu_id, true,
                IncognitoUtils.isIncognitoModeEnabled(), IncognitoUtils.isIncognitoModeManaged());
    }

    /**
     * Sets the visibility and labels of the "Add to Home screen" and "Open WebAPK" menu items.
     */
    protected void prepareAddToHomescreenMenuItem(
            Menu menu, Tab currentTab, boolean canShowHomeScreenMenuItem) {
        MenuItem homescreenItem = menu.findItem(R.id.add_to_homescreen_id);
        MenuItem openWebApkItem = menu.findItem(R.id.open_webapk_id);
        if (canShowHomeScreenMenuItem) {
            Context context = ContextUtils.getApplicationContext();
            long addToHomeScreenStart = SystemClock.elapsedRealtime();
            ResolveInfo resolveInfo =
                    WebApkValidator.queryFirstWebApkResolveInfo(context, currentTab.getUrl());
            RecordHistogram.recordTimesHistogram("Android.PrepareMenu.OpenWebApkVisibilityCheck",
                    SystemClock.elapsedRealtime() - addToHomeScreenStart);

            boolean openWebApkItemVisible =
                    resolveInfo != null && resolveInfo.activityInfo.packageName != null;

            if (openWebApkItemVisible) {
                String appName = resolveInfo.loadLabel(context.getPackageManager()).toString();
                openWebApkItem.setTitle(context.getString(R.string.menu_open_webapk, appName));

                homescreenItem.setVisible(false);
                openWebApkItem.setVisible(true);
            } else {
                homescreenItem.setTitle(AppBannerManager.getHomescreenLanguageOption());
                homescreenItem.setVisible(true);
                openWebApkItem.setVisible(false);
            }
        } else {
            homescreenItem.setVisible(false);
            openWebApkItem.setVisible(false);
        }
    }

    @Override
    public Bundle getBundleForMenuItem(MenuItem item) {
        return null;
    }

    /**
     * Sets the visibility of the "Translate" menu item.
     */
    protected void prepareTranslateMenuItem(Menu menu, Tab currentTab) {
        boolean isTranslateVisible = TranslateUtils.canTranslateCurrentTab(currentTab);
        RecordHistogram.recordBooleanHistogram(
                "Translate.MobileMenuTranslate.Shown", isTranslateVisible);
        menu.findItem(R.id.translate_id).setVisible(isTranslateVisible);
    }

    @Override
    public void loadingStateChanged(boolean isLoading) {
        if (mReloadMenuItem != null) {
            Resources resources = mContext.getResources();
            mReloadMenuItem.getIcon().setLevel(isLoading
                            ? resources.getInteger(R.integer.reload_button_level_stop)
                            : resources.getInteger(R.integer.reload_button_level_reload));
            mReloadMenuItem.setTitle(isLoading ? R.string.accessibility_btn_stop_loading
                                               : R.string.accessibility_btn_refresh);
            mReloadMenuItem.setTitleCondensed(resources.getString(
                    isLoading ? R.string.menu_stop_refresh : R.string.menu_refresh));
        }
    }

    @Override
    public void onMenuDismissed() {
        mReloadMenuItem = null;
        if (mUpdateMenuItemVisible) {
            UpdateMenuItemHelper.getInstance().onMenuDismissed();
            UpdateMenuItemHelper.getInstance().unregisterObserver(mAppMenuInvalidator);
            mUpdateMenuItemVisible = false;
            mAppMenuInvalidator = null;
        }
    }

    // Set enabled to be |enable| for all MenuItems with |id| in |menu|.
    // If |managed| is true then the "Managed By Enterprise" icon is shown next to the menu.
    private void disableEnableMenuItem(
            Menu menu, int id, boolean visible, boolean enabled, boolean managed) {
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == id && item.isVisible()) {
                item.setVisible(visible);
                item.setEnabled(enabled);
                if (managed) {
                    item.setIcon(ManagedPreferencesUtils.getManagedByEnterpriseIconId());
                } else {
                    item.setIcon(null);
                }
            }
        }
    }

    @Override
    public int getFooterResourceId() {
        return 0;
    }

    @Override
    public int getHeaderResourceId() {
        return 0;
    }

    @Override
    public boolean shouldShowFooter(int maxMenuHeight) {
        return true;
    }

    @Override
    public boolean shouldShowHeader(int maxMenuHeight) {
        return true;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {}

    @Override
    public void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view) {}

    /**
     * Updates the bookmark item's visibility.
     *
     * @param bookmarkMenuItem {@link MenuItem} for adding/editing the bookmark.
     * @param currentTab        Current tab being displayed.
     */
    protected void updateBookmarkMenuItem(MenuItem bookmarkMenuItem, Tab currentTab) {
        bookmarkMenuItem.setEnabled(mBookmarkBridge.isEditBookmarksEnabled());
        if (BookmarkBridge.hasBookmarkIdForTab(currentTab)) {
            bookmarkMenuItem.setIcon(R.drawable.btn_star_filled);
            bookmarkMenuItem.setChecked(true);
            bookmarkMenuItem.setTitleCondensed(mContext.getString(R.string.edit_bookmark));
        } else {
            bookmarkMenuItem.setIcon(R.drawable.btn_star);
            bookmarkMenuItem.setChecked(false);
            bookmarkMenuItem.setTitleCondensed(mContext.getString(R.string.menu_bookmark));
        }
    }

    /**
     * Updates the request desktop site item's state.
     *
     * @param menu {@link Menu} for request desktop site.
     * @param currentTab      Current tab being displayed.
     */
    protected void updateRequestDesktopSiteMenuItem(
            Menu menu, Tab currentTab, boolean canShowRequestDekstopSite) {
        MenuItem requestMenuRow = menu.findItem(R.id.request_desktop_site_row_menu_id);
        MenuItem requestMenuLabel = menu.findItem(R.id.request_desktop_site_id);
        MenuItem requestMenuCheck = menu.findItem(R.id.request_desktop_site_check_id);

        // Hide request desktop site on all chrome:// pages except for the NTP.
        String url = currentTab.getUrl();
        boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
        // Also hide request desktop site on Reader Mode.
        boolean isDistilledPage = DomDistillerUrlUtils.isDistilledPage(url);

        boolean itemVisible = canShowRequestDekstopSite
                && (!isChromeScheme || currentTab.isNativePage()) && !isDistilledPage;
        requestMenuRow.setVisible(itemVisible);
        if (!itemVisible) return;

        boolean isRds =
                currentTab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        // Mark the checkbox if RDS is activated on this page.
        requestMenuCheck.setChecked(isRds);

        // This title doesn't seem to be displayed by Android, but it is used to set up
        // accessibility text in {@link AppMenuAdapter#setupMenuButton}.
        requestMenuLabel.setTitleCondensed(isRds
                        ? mContext.getString(R.string.menu_request_desktop_site_on)
                        : mContext.getString(R.string.menu_request_desktop_site_off));
    }
}
