// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/**
 * Base implementation of {@link AppMenuPropertiesDelegate} that handles hiding and showing menu
 * items based on activity state.
 */
public class AppMenuPropertiesDelegateImpl implements AppMenuPropertiesDelegate {
    public static final StringCachedFieldTrialParameter ACTION_BAR_VARIATION =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, "action_bar", "");

    private static Boolean sItemBookmarkedForTesting;

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
    private ShareUtils mShareUtils;
    @VisibleForTesting
    @IntDef({MenuGroup.INVALID, MenuGroup.PAGE_MENU, MenuGroup.OVERVIEW_MODE_MENU,
            MenuGroup.START_SURFACE_MODE_MENU, MenuGroup.TABLET_EMPTY_MODE_MENU})
    @interface MenuGroup {
        int INVALID = -1;
        int PAGE_MENU = 0;
        int OVERVIEW_MODE_MENU = 1;
        int START_SURFACE_MODE_MENU = 2;
        int TABLET_EMPTY_MODE_MENU = 3;
    }

    @IntDef({ActionBarType.STANDARD, ActionBarType.BACKWARD_BUTTON, ActionBarType.SHARE_BUTTON})
    @interface ActionBarType {
        int STANDARD = 0;
        int BACKWARD_BUTTON = 1;
        int SHARE_BUTTON = 2;
    }

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
        mShareUtils = new ShareUtils();
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
        if (shouldShowRegroupedMenu()) {
            return R.menu.main_menu_regroup;
        }
        return R.menu.main_menu;
    }

    @Override
    public @Nullable List<CustomViewBinder> getCustomViewBinders() {
        List<CustomViewBinder> customViewBinders = new ArrayList<>();
        customViewBinders.add(new UpdateMenuItemViewBinder());
        customViewBinders.add(new ManagedByMenuItemViewBinder());
        customViewBinders.add(new IncognitoMenuItemViewBinder());
        customViewBinders.add(new DividerLineMenuItemViewBinder());
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

    @VisibleForTesting
    @MenuGroup
    int getMenuGroup() {
        // Determine which menu to show.
        @MenuGroup
        int menuGroup = MenuGroup.INVALID;
        if (shouldShowPageMenu()) menuGroup = MenuGroup.PAGE_MENU;

        boolean isOverview =
                mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible();
        if (mIsTablet) {
            boolean hasTabs = mTabModelSelector.getCurrentModel().getCount() != 0;
            if (hasTabs && isOverview) {
                menuGroup = MenuGroup.OVERVIEW_MODE_MENU;
            } else if (!hasTabs) {
                menuGroup = MenuGroup.TABLET_EMPTY_MODE_MENU;
            }
        } else if (isOverview) {
            menuGroup = StartSurfaceConfiguration.isStartSurfaceEnabled()
                    ? MenuGroup.START_SURFACE_MODE_MENU
                    : MenuGroup.OVERVIEW_MODE_MENU;
        }
        assert menuGroup != MenuGroup.INVALID;
        return menuGroup;
    }

    private void setMenuGroupVisibility(@MenuGroup int menuGroup, Menu menu) {
        menu.setGroupVisible(R.id.PAGE_MENU, menuGroup == MenuGroup.PAGE_MENU);
        menu.setGroupVisible(R.id.OVERVIEW_MODE_MENU, menuGroup == MenuGroup.OVERVIEW_MODE_MENU);
        menu.setGroupVisible(
                R.id.START_SURFACE_MODE_MENU, menuGroup == MenuGroup.START_SURFACE_MODE_MENU);
        menu.setGroupVisible(
                R.id.TABLET_EMPTY_MODE_MENU, menuGroup == MenuGroup.TABLET_EMPTY_MODE_MENU);
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        int menuGroup = getMenuGroup();
        setMenuGroupVisibility(menuGroup, menu);

        boolean isIncognito = mTabModelSelector.getCurrentModel().isIncognito();
        Tab currentTab = mActivityTabProvider.get();

        if (menuGroup == MenuGroup.PAGE_MENU && currentTab != null) {
            preparePageMenu(menu, currentTab, handler, isIncognito);
        }
        prepareCommonMenuItems(menu, menuGroup, isIncognito);
    }

    private void preparePageMenu(
            Menu menu, Tab currentTab, AppMenuHandler handler, boolean isIncognito) {
        String url = currentTab.getUrlString();
        boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
        boolean isFileScheme = url.startsWith(UrlConstants.FILE_URL_PREFIX);
        boolean isContentScheme = url.startsWith(UrlConstants.CONTENT_URL_PREFIX);

        // Update the icon row items (shown in narrow form factors).
        boolean shouldShowIconRow = shouldShowIconRow();
        menu.findItem(R.id.icon_row_menu_id).setVisible(shouldShowIconRow);
        if (shouldShowIconRow) {
            SubMenu actionBar = menu.findItem(R.id.icon_row_menu_id).getSubMenu();

            @ActionBarType
            int actionBarType = getActionBarType();
            MenuItem backwardMenuItem = actionBar.findItem(R.id.backward_menu_id);
            if (backwardMenuItem != null) {
                if (actionBarType == ActionBarType.BACKWARD_BUTTON) {
                    backwardMenuItem.setEnabled(currentTab.canGoBack());
                } else {
                    actionBar.removeItem(R.id.backward_menu_id);
                }
            }

            // Disable the "Forward" menu item if there is no page to go to.
            MenuItem forwardMenuItem = actionBar.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(currentTab.canGoForward());

            mReloadMenuItem = actionBar.findItem(R.id.reload_menu_id);
            Drawable icon = AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
            DrawableCompat.setTintList(icon,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_tint_list));
            mReloadMenuItem.setIcon(icon);
            loadingStateChanged(currentTab.isLoading());

            MenuItem bookmarkMenuItem = actionBar.findItem(R.id.bookmark_this_page_id);
            updateBookmarkMenuItem(bookmarkMenuItem, currentTab);

            MenuItem offlineMenuItem = actionBar.findItem(R.id.offline_page_id);
            if (offlineMenuItem != null) {
                offlineMenuItem.setEnabled(shouldEnableDownloadPage(currentTab));
            }

            MenuItem shareMenuItem = actionBar.findItem(R.id.share_menu_button_id);
            if (shareMenuItem != null) {
                if (actionBarType == ActionBarType.SHARE_BUTTON) {
                    shareMenuItem.setEnabled(mShareUtils.shouldEnableShare(currentTab));
                } else {
                    actionBar.removeItem(R.id.share_menu_button_id);
                }
            }

            if (actionBarType != ActionBarType.STANDARD) {
                actionBar.removeItem(R.id.info_menu_id);
            }

            assert actionBar.size() == 5;
        }

        mUpdateMenuItemVisible = shouldShowUpdateMenuItem();
        menu.findItem(R.id.update_menu_id).setVisible(mUpdateMenuItemVisible);
        if (mUpdateMenuItemVisible) {
            mAppMenuInvalidator = () -> handler.invalidateAppMenu();
            UpdateMenuItemHelper.getInstance().registerObserver(mAppMenuInvalidator);
        }

        menu.findItem(R.id.move_to_other_window_menu_id).setVisible(shouldShowMoveToOtherWindow());

        // Don't allow either "chrome://" pages or interstitial pages to be shared.
        menu.findItem(R.id.share_row_menu_id)
                .setVisible(mShareUtils.shouldEnableShare(currentTab) && shouldShowShareInMenu());

        ShareHelper.configureDirectShareMenuItem(
                mContext, menu.findItem(R.id.direct_share_menu_id));

        menu.findItem(R.id.paint_preview_show_id)
                .setVisible(shouldShowPaintPreview(isChromeScheme, currentTab, isIncognito));

        // Disable find in page on the native NTP.
        menu.findItem(R.id.find_in_page_id).setVisible(shouldShowFindInPage(currentTab));

        // Prepare translate menu button.
        prepareTranslateMenuItem(menu, currentTab);

        prepareAddToHomescreenMenuItem(menu, currentTab,
                shouldShowHomeScreenMenuItem(
                        isChromeScheme, isFileScheme, isContentScheme, isIncognito, url));

        updateRequestDesktopSiteMenuItem(menu, currentTab, true /* can show */);

        // Only display reader mode settings menu option if the current page is in reader mode.
        menu.findItem(R.id.reader_mode_prefs_id).setVisible(shouldShowReaderModePrefs(currentTab));

        MenuItem infoMenuItem = menu.findItem(R.id.info_id);
        if (infoMenuItem != null) {
            infoMenuItem.setVisible(shouldShowInfoInMenu());
        }

        // Only display the Enter VR button if VR Shell Dev environment is enabled.
        menu.findItem(R.id.enter_vr_id).setVisible(shouldShowEnterVr());

        MenuItem managedByMenuItem = menu.findItem(R.id.managed_by_menu_id);
        managedByMenuItem.setVisible(shouldShowManagedByMenuItem(currentTab));
        // TODO(https://crbug.com/1092175): Enable "managed by" menu item after chrome://management
        // page is added.
        managedByMenuItem.setEnabled(false);
    }

    private void prepareCommonMenuItems(Menu menu, @MenuGroup int menuGroup, boolean isIncognito) {
        // We have to iterate all menu items since same menu item ID may be associated with more
        // than one menu items.
        boolean isMenuGroupTabsVisible = TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                && !DeviceClassManager.enableAccessibilityLayout();
        boolean isMenuGroupTabsEnabled = isMenuGroupTabsVisible
                && mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter()
                                .getTabsWithNoOtherRelatedTabs()
                                .size()
                        > 1;

        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (!shouldShowIconBeforeItem()) {
                // Remove icons for menu items except the reader mode prefs and the update menu
                // item.
                if (item.getItemId() != R.id.reader_mode_prefs_id
                        && item.getItemId() != R.id.update_menu_id) {
                    item.setIcon(null);
                }
            }

            if (item.getItemId() == R.id.new_incognito_tab_menu_id && item.isVisible()) {
                // Disable new incognito tab when it is blocked (e.g. by a policy).
                // findItem(...).setEnabled(...)" is not enough here, because of the inflated
                // main_menu.xml contains multiple items with the same id in different groups
                // e.g.: menu_new_incognito_tab.
                item.setEnabled(isIncognitoEnabled());
            }

            if (item.getItemId() == R.id.divider_line_id) {
                item.setEnabled(false);
            }

            int itemGroupId = item.getGroupId();
            if (!(menuGroup == MenuGroup.START_SURFACE_MODE_MENU
                                && itemGroupId == R.id.START_SURFACE_MODE_MENU
                        || menuGroup == MenuGroup.OVERVIEW_MODE_MENU
                                && itemGroupId == R.id.OVERVIEW_MODE_MENU
                        || menuGroup == MenuGroup.PAGE_MENU && itemGroupId == R.id.PAGE_MENU)) {
                continue;
            }

            if (item.getItemId() == R.id.recent_tabs_menu_id) {
                item.setVisible(!isIncognito);
            }
            if (item.getItemId() == R.id.menu_group_tabs) {
                item.setVisible(isMenuGroupTabsVisible);
                item.setEnabled(isMenuGroupTabsEnabled);
            }
            if (item.getItemId() == R.id.close_all_tabs_menu_id) {
                boolean hasTabs = mTabModelSelector.getTotalTabCount() > 0;
                item.setVisible(!isIncognito);
                item.setEnabled(hasTabs);
            }
            if (item.getItemId() == R.id.close_all_incognito_tabs_menu_id) {
                boolean hasIncognitoTabs = mTabModelSelector.getModel(true).getCount() > 0;
                item.setVisible(isIncognito);
                item.setEnabled(hasIncognitoTabs);
            }
        }
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the reader mode preferences menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowReaderModePrefs(@NonNull Tab currentTab) {
        return DomDistillerUrlUtils.isDistilledPage(currentTab.getUrlString());
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the {@code currentTab} may be downloaded, indicating whether the download
     *         page menu item should be enabled.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldEnableDownloadPage(@NonNull Tab currentTab) {
        return DownloadUtils.isAllowedToDownloadPage(currentTab);
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether bookmark page menu item should be checked, indicating that the current tab
     *         is bookmarked.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldCheckBookmarkStar(@NonNull Tab currentTab) {
        return sItemBookmarkedForTesting != null
                ? sItemBookmarkedForTesting
                : mBookmarkBridge != null && mBookmarkBridge.hasBookmarkIdForTab(currentTab);
    }

    /**
     * @return Whether the update Chrome menu item should be displayed.
     */
    protected boolean shouldShowUpdateMenuItem() {
        return UpdateMenuItemHelper.getInstance().getUiState().itemState != null;
    }

    /**
     * @return Whether the "Move to other window" menu item should be displayed.
     */
    protected boolean shouldShowMoveToOtherWindow() {
        boolean hasMoreThanOneTab = mTabModelSelector.getTotalTabCount() > 1;
        return mMultiWindowModeStateDispatcher.isOpenInOtherWindowSupported() && hasMoreThanOneTab;
    }

    /**
     * @param isChromeScheme Whether URL for the current tab starts with the chrome:// scheme.
     * @param currentTab The currentTab for which the app menu is showing.
     * @param isIncognito Whether the currentTab is incognito.
     * @return Whether the paint preview menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowPaintPreview(
            boolean isChromeScheme, @NonNull Tab currentTab, boolean isIncognito) {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_DEMO) && !isChromeScheme
                && !isIncognito;
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the find in page menu item should be displayed.
     */
    protected boolean shouldShowFindInPage(@NonNull Tab currentTab) {
        return !currentTab.isNativePage() && currentTab.getWebContents() != null;
    }

    /**
     * @return Whether the enter VR menu item should be displayed.
     */
    protected boolean shouldShowEnterVr() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_VR_SHELL_DEV);
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the translate menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowTranslateMenuItem(@NonNull Tab currentTab) {
        return TranslateUtils.canTranslateCurrentTab(currentTab);
    }

    /**
     * @param isChromeScheme Whether URL for the current tab starts with the chrome:// scheme.
     * @param isFileScheme Whether URL for the current tab starts with the file:// scheme.
     * @param isContentScheme Whether URL for the current tab starts with the file:// scheme.
     * @param isIncognito Whether the current tab is incognito.
     * @param url The URL for the current tab.
     * @return Whether the homescreen menu item should be displayed.
     */
    protected boolean shouldShowHomeScreenMenuItem(boolean isChromeScheme, boolean isFileScheme,
            boolean isContentScheme, boolean isIncognito, String url) {
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
        return ShortcutHelper.isAddToHomeIntentSupported() && !isChromeScheme && !isFileScheme
                && !isContentScheme && !isIncognito && !TextUtils.isEmpty(url);
    }

    /**
     * @param currentTab Current tab being displayed.
     * @return Whether the "Managed by your organization" menu item should be displayed.
     */
    protected boolean shouldShowManagedByMenuItem(Tab currentTab) {
        return false;
    }

    /**
     * Sets the visibility and labels of the "Add to Home screen" and "Open WebAPK" menu items.
     */
    protected void prepareAddToHomescreenMenuItem(
            Menu menu, Tab currentTab, boolean shouldShowHomeScreenMenuItem) {
        MenuItem homescreenItem = menu.findItem(R.id.add_to_homescreen_id);
        MenuItem openWebApkItem = menu.findItem(R.id.open_webapk_id);
        if (shouldShowHomeScreenMenuItem) {
            Context context = ContextUtils.getApplicationContext();
            long addToHomeScreenStart = SystemClock.elapsedRealtime();
            ResolveInfo resolveInfo =
                    WebApkValidator.queryFirstWebApkResolveInfo(context, currentTab.getUrlString());
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
                homescreenItem.setTitle(getAddToHomeScreenTitle());
                homescreenItem.setVisible(true);
                openWebApkItem.setVisible(false);
            }
        } else {
            homescreenItem.setVisible(false);
            openWebApkItem.setVisible(false);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public @StringRes int getAddToHomeScreenTitle() {
        return AppBannerManager.getHomescreenLanguageOption();
    }

    @Override
    public Bundle getBundleForMenuItem(MenuItem item) {
        return null;
    }

    /**
     * Sets the visibility of the "Translate" menu item.
     */
    protected void prepareTranslateMenuItem(Menu menu, Tab currentTab) {
        boolean isTranslateVisible = shouldShowTranslateMenuItem(currentTab);
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

    @VisibleForTesting
    boolean shouldShowIconRow() {
        boolean shouldShowIconRow = !mIsTablet
                || mDecorView.getWidth()
                        < DeviceFormFactor.getNonMultiDisplayMinimumTabletWidthPx(mContext);

        final boolean isMenuButtonOnTop =
                mToolbarManager != null && !mToolbarManager.isMenuFromBottom();
        shouldShowIconRow &= isMenuButtonOnTop;
        return shouldShowIconRow;
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
    public int getGroupDividerId() {
        return R.id.divider_line_id;
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

    @Override
    public boolean shouldShowIconBeforeItem() {
        return false;
    }

    @Override
    public boolean shouldShowRegroupedMenu() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP);
    }

    /**
     * Updates the bookmark item's visibility.
     *
     * @param bookmarkMenuItem {@link MenuItem} for adding/editing the bookmark.
     * @param currentTab        Current tab being displayed.
     */
    protected void updateBookmarkMenuItem(MenuItem bookmarkMenuItem, Tab currentTab) {
        // If this method is called before the {@link #mBookmarkBridgeSupplierCallback} has been
        // called, try to retrieve the bridge directly from the supplier.
        if (mBookmarkBridge == null && mBookmarkBridgeSupplier != null) {
            mBookmarkBridge = mBookmarkBridgeSupplier.get();
        }

        if (mBookmarkBridge == null) {
            // If the BookmarkBridge still isn't available, assume the bookmark menu item is not
            // editable.
            bookmarkMenuItem.setEnabled(false);
        } else {
            bookmarkMenuItem.setEnabled(mBookmarkBridge.isEditBookmarksEnabled());
        }

        if (shouldCheckBookmarkStar(currentTab)) {
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
            Menu menu, Tab currentTab, boolean canShowRequestDesktopSite) {
        MenuItem requestMenuRow = menu.findItem(R.id.request_desktop_site_row_menu_id);
        MenuItem requestMenuLabel = menu.findItem(R.id.request_desktop_site_id);
        MenuItem requestMenuCheck = menu.findItem(R.id.request_desktop_site_check_id);

        // Hide request desktop site on all chrome:// pages except for the NTP.
        String url = currentTab.getUrlString();
        boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);

        boolean itemVisible = canShowRequestDesktopSite
                && (!isChromeScheme || currentTab.isNativePage())
                && !shouldShowReaderModePrefs(currentTab) && currentTab.getWebContents() != null;
        requestMenuRow.setVisible(itemVisible);
        if (!itemVisible) return;

        boolean isRequestDesktopSite =
                currentTab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        // Mark the checkbox if RDS is activated on this page.
        requestMenuCheck.setChecked(isRequestDesktopSite);

        // This title doesn't seem to be displayed by Android, but it is used to set up
        // accessibility text in {@link AppMenuAdapter#setupMenuButton}.
        requestMenuLabel.setTitleCondensed(isRequestDesktopSite
                        ? mContext.getString(R.string.menu_request_desktop_site_on)
                        : mContext.getString(R.string.menu_request_desktop_site_off));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean isIncognitoEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled();
    }

    @VisibleForTesting
    static void setPageBookmarkedForTesting(Boolean bookmarked) {
        sItemBookmarkedForTesting = bookmarked;
    }

    private boolean shouldShowShareInMenu() {
        return getActionBarType() != ActionBarType.SHARE_BUTTON;
    }

    private boolean shouldShowInfoInMenu() {
        return getActionBarType() != ActionBarType.STANDARD;
    }

    /**
     * @return The type of action bar should be shown.
     */
    private @ActionBarType int getActionBarType() {
        if (ACTION_BAR_VARIATION.getValue().equals("backward_button")) {
            return ActionBarType.BACKWARD_BUTTON;
        } else if (ACTION_BAR_VARIATION.getValue().equals("share_button")) {
            return ActionBarType.SHARE_BUTTON;
        }
        return ActionBarType.STANDARD;
    }
}
