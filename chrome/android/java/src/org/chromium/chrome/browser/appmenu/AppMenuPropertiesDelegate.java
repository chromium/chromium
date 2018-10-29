// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.webapk.lib.client.WebApkValidator;

import java.util.concurrent.TimeUnit;

/**
 * App Menu helper that handles hiding and showing menu items based on activity state.
 */
public class AppMenuPropertiesDelegate {
    protected MenuItem mReloadMenuItem;

    protected final ChromeActivity mActivity;

    protected BookmarkBridge mBookmarkBridge;

    public AppMenuPropertiesDelegate(ChromeActivity activity) {
        mActivity = activity;
    }

    /**
     * @return Whether the App Menu should be shown.
     */
    public boolean shouldShowAppMenu() {
        return mActivity.shouldShowAppMenu();
    }

    /**
     * @return Whether the app menu for a web page should be shown.
     */
    public boolean shouldShowPageMenu() {
        boolean isOverview = mActivity.isInOverviewMode();

        if (mActivity.isTablet()) {
            boolean hasTabs = mActivity.getCurrentTabModel().getCount() != 0;
            return hasTabs && !isOverview;
        } else {
            return !isOverview && mActivity.getActivityTab() != null;
        }
    }

    /**
     * Allows the delegate to show and hide items before the App Menu is shown. It is called every
     * time the menu is shown. This assumes that the provided menu contains all the items expected
     * in the application menu (i.e. that the main menu has been inflated into it).
     * @param menu Menu that will be used as the source for the App Menu pop up.
     */
    public void prepareMenu(Menu menu) {
        // Exactly one of these will be true, depending on the type of menu showing.
        boolean isPageMenu = shouldShowPageMenu();
        boolean isOverviewMenu;
        boolean isTabletEmptyModeMenu;

        boolean isOverview = mActivity.isInOverviewMode();
        boolean isIncognito = mActivity.getCurrentTabModel().isIncognito();
        Tab currentTab = mActivity.getActivityTab();

        // Determine which menu to show.
        if (mActivity.isTablet()) {
            boolean hasTabs = mActivity.getCurrentTabModel().getCount() != 0;
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
            boolean shouldShowIconRow = !mActivity.isTablet()
                    || mActivity.getWindow().getDecorView().getWidth()
                            < DeviceFormFactor.getMinimumTabletWidthPx(
                                      mActivity.getWindowAndroid().getDisplay());

            boolean bottomToolbarEnabled = mActivity.getToolbarManager() != null
                    && mActivity.getToolbarManager().getBottomToolbarCoordinator() != null;
            shouldShowIconRow &= !bottomToolbarEnabled;

            // Update the icon row items (shown in narrow form factors).
            menu.findItem(R.id.icon_row_menu_id).setVisible(shouldShowIconRow);
            if (shouldShowIconRow) {
                // Disable the "Forward" menu item if there is no page to go to.
                MenuItem forwardMenuItem = menu.findItem(R.id.forward_menu_id);
                forwardMenuItem.setEnabled(currentTab.canGoForward());

                mReloadMenuItem = menu.findItem(R.id.reload_menu_id);
                Drawable icon =
                        AppCompatResources.getDrawable(mActivity, R.drawable.btn_reload_stop);
                DrawableCompat.setTintList(icon,
                        AppCompatResources.getColorStateList(mActivity, R.color.dark_mode_tint));
                mReloadMenuItem.setIcon(icon);
                loadingStateChanged(currentTab.isLoading());

                MenuItem bookmarkMenuItem = menu.findItem(R.id.bookmark_this_page_id);
                updateBookmarkMenuItem(bookmarkMenuItem, currentTab);

                MenuItem offlineMenuItem = menu.findItem(R.id.offline_page_id);
                if (offlineMenuItem != null) {
                    offlineMenuItem.setEnabled(
                            DownloadUtils.isAllowedToDownloadPage(currentTab));
                }
            }

            menu.findItem(R.id.update_menu_id).setVisible(
                    UpdateMenuItemHelper.getInstance().shouldShowMenuItem(mActivity));

            boolean hasMoreThanOneTab = mActivity.getTabModelSelector().getTotalTabCount() > 1;
            menu.findItem(R.id.move_to_other_window_menu_id).setVisible(
                    MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity)
                    && hasMoreThanOneTab);

            MenuItem recentTabsMenuItem = menu.findItem(R.id.recent_tabs_menu_id);
            recentTabsMenuItem.setVisible(!isIncognito);
            recentTabsMenuItem.setTitle(R.string.menu_recent_tabs);

            MenuItem allBookmarksMenuItem = menu.findItem(R.id.all_bookmarks_menu_id);
            allBookmarksMenuItem.setTitle(mActivity.getString(R.string.menu_bookmarks));

            // Don't allow either "chrome://" pages or interstitial pages to be shared.
            menu.findItem(R.id.share_row_menu_id)
                    .setVisible(!isChromeScheme && !currentTab.isShowingInterstitialPage());

            ShareHelper.configureDirectShareMenuItem(
                    mActivity, menu.findItem(R.id.direct_share_menu_id));

            // Disable find in page on the native NTP.
            menu.findItem(R.id.find_in_page_id).setVisible(
                    !currentTab.isNativePage() && currentTab.getWebContents() != null);

            // Prepare translate menu button.
            boolean isTranslateVisible = !isChromeScheme && !isFileScheme && !isContentScheme
                    && !TextUtils.isEmpty(url) && currentTab.getWebContents() != null
                    && ChromeFeatureList.isInitialized()
                    && ChromeFeatureList.isEnabled(
                               ChromeFeatureList.TRANSLATE_ANDROID_MANUAL_TRIGGER)
                    && TranslateBridge.canManuallyTranslate(currentTab);
            if (ChromeFeatureList.isInitialized()
                    && ChromeFeatureList.isEnabled(
                               ChromeFeatureList.TRANSLATE_ANDROID_MANUAL_TRIGGER)) {
                RecordHistogram.recordBooleanHistogram(
                        "Translate.MobileMenuTranslate.Shown", isTranslateVisible);
            }
            menu.findItem(R.id.translate_id).setVisible(isTranslateVisible);

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
            menu.findItem(R.id.enter_vr_id).setVisible(
                    CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_VR_SHELL_DEV));
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
                menu.findItem(R.id.close_all_tabs_menu_id).setEnabled(
                        mActivity.getTabModelSelector().getTotalTabCount() > 0);
            }
        }

        // We have to iterate all menu items since same menu item ID may be associated with more
        // than one menu items.
        boolean useAlternativeIncognitoStrings =
                ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS);
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (item.getItemId() == R.id.new_incognito_tab_menu_id) {
                item.setTitle(useAlternativeIncognitoStrings ? R.string.menu_new_private_tab
                                                             : R.string.menu_new_incognito_tab);
            } else if (item.getItemId() == R.id.close_all_incognito_tabs_menu_id) {
                item.setTitle(useAlternativeIncognitoStrings
                                ? R.string.menu_close_all_private_tabs
                                : R.string.menu_close_all_incognito_tabs);
            }
        }

        // Disable new incognito tab when it is blocked (e.g. by a policy).
        // findItem(...).setEnabled(...)" is not enough here, because of the inflated
        // main_menu.xml contains multiple items with the same id in different groups
        // e.g.: new_incognito_tab_menu_id.
        disableEnableMenuItem(menu, R.id.new_incognito_tab_menu_id, true,
                PrefServiceBridge.getInstance().isIncognitoModeEnabled(),
                PrefServiceBridge.getInstance().isIncognitoModeManaged());
        mActivity.prepareMenu(menu);
    }

    /**
     * Sets the visibility and labels of the "Add to Home screen" and "Open WebAPK" menu items.
     */
    protected void prepareAddToHomescreenMenuItem(
            Menu menu, Tab currentTab, boolean canShowHomeScreenMenuItem) {
        // Record whether or not we have finished installability checks for this page when we're
        // preparing the menu to be displayed. This will let us determine if it is feasible to
        // change the add to homescreen menu item based on whether a site is a PWA.
        currentTab.getAppBannerManager().recordMenuOpen();

        MenuItem homescreenItem = menu.findItem(R.id.add_to_homescreen_id);
        MenuItem openWebApkItem = menu.findItem(R.id.open_webapk_id);
        if (canShowHomeScreenMenuItem) {
            Context context = ContextUtils.getApplicationContext();
            long addToHomeScreenStart = SystemClock.elapsedRealtime();
            ResolveInfo resolveInfo =
                    WebApkValidator.queryWebApkResolveInfo(context, currentTab.getUrl());
            RecordHistogram.recordTimesHistogram("Android.PrepareMenu.OpenWebApkVisibilityCheck",
                    SystemClock.elapsedRealtime() - addToHomeScreenStart, TimeUnit.MILLISECONDS);

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

    /**
     * Notify the delegate that the load state changed.
     * @param isLoading Whether the page is currently loading.
     */
    public void loadingStateChanged(boolean isLoading) {
        if (mReloadMenuItem != null) {
            Resources resources = mActivity.getResources();
            mReloadMenuItem.getIcon().setLevel(isLoading
                            ? resources.getInteger(R.integer.reload_button_level_stop)
                            : resources.getInteger(R.integer.reload_button_level_reload));
            mReloadMenuItem.setTitle(isLoading
                    ? R.string.accessibility_btn_stop_loading : R.string.accessibility_btn_refresh);
        }
    }

    /**
     * Notify the delegate that menu was dismissed.
     */
    public void onMenuDismissed() {
        mReloadMenuItem = null;
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

    /**
     * @return Resource layout id for the footer if there should be one. O otherwise. The footer
     *         is shown at a fixed position at the bottom the app menu. It is always visible and
     *         overlays other app menu items if necessary.
     */
    public int getFooterResourceId() {
        return 0;
    }

    /**
     * @return The resource ID for a layout the be used as the app menu header if there should be
     *         one. 0 otherwise. The header will be displayed as the first item in the app menu. It
     *         will be scrolled off as the menu scrolls.
     */
    public int getHeaderResourceId() {
        return 0;
    }

    /**
     * @return The {@link OnClickListener} to notify when the header view is clicked. May be null if
     *         nothing should happen when the header is clicked.
     */
    @Nullable
    public OnClickListener getHeaderOnClickListener() {
        return null;
    }

    /**
     * Determines whether the footer should be shown based on the maximum available menu height.
     * @param maxMenuHeight The maximum available height for the menu to draw.
     * @return Whether the footer, as specified in {@link #getFooterResourceId()}, should be shown.
     */
    public boolean shouldShowFooter(int maxMenuHeight) {
        return true;
    }

    /**
     * Determines whether the header should be shown based on the maximum available menu height.
     * @param maxMenuHeight The maximum available height for the menu to draw.
     * @return Whether the header, as specified in {@link #getHeaderView()}, should be shown.
     */
    public boolean shouldShowHeader(int maxMenuHeight) {
        return true;
    }

    /**
     * Updates the bookmarks bridge.
     *
     * @param bookmarkBridge The bookmarks bridge.
     */
    public void setBookmarkBridge(BookmarkBridge bookmarkBridge) {
        mBookmarkBridge = bookmarkBridge;
    }

    /**
     * Updates the bookmark item's visibility.
     *
     * @param bookmarkMenuItem {@link MenuItem} for adding/editing the bookmark.
     * @param currentTab        Current tab being displayed.
     */
    protected void updateBookmarkMenuItem(MenuItem bookmarkMenuItem, Tab currentTab) {
        bookmarkMenuItem.setEnabled(mBookmarkBridge.isEditBookmarksEnabled());
        if (currentTab.getBookmarkId() != Tab.INVALID_BOOKMARK_ID) {
            bookmarkMenuItem.setIcon(R.drawable.btn_star_filled);
            bookmarkMenuItem.setChecked(true);
            bookmarkMenuItem.setTitleCondensed(mActivity.getString(R.string.edit_bookmark));
        } else {
            bookmarkMenuItem.setIcon(R.drawable.btn_star);
            bookmarkMenuItem.setChecked(false);
            bookmarkMenuItem.setTitleCondensed(null);
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

        boolean isRds = currentTab.getUseDesktopUserAgent();
        // Mark the checkbox if RDS is activated on this page.
        requestMenuCheck.setChecked(isRds);

        // This title doesn't seem to be displayed by Android, but it is used to set up
        // accessibility text in {@link AppMenuAdapter#setupMenuButton}.
        requestMenuLabel.setTitleCondensed(isRds
                        ? mActivity.getString(R.string.menu_request_desktop_site_on)
                        : mActivity.getString(R.string.menu_request_desktop_site_off));
    }

    /**
     * A notification that the header view has finished inflating.
     * @param view The view that was inflated.
     * @param appMenu The menu the view is inside of.
     */
    public void onHeaderViewInflated(AppMenu appMenu, View view) {}

    /**
     * A notification that the footer view has finished inflating.
     * @param view The view that was inflated.
     * @param appMenu The menu the view is inside of.
     */
    public void onFooterViewInflated(AppMenu appMenu, View view) {}
}
