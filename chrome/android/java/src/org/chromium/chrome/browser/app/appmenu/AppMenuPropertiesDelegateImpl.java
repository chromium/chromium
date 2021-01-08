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
import android.util.Pair;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.banners.AppBannerManager;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
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
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Base implementation of {@link AppMenuPropertiesDelegate} that handles hiding and showing menu
 * items based on activity state.
 */
public class AppMenuPropertiesDelegateImpl implements AppMenuPropertiesDelegate {
    public static final StringCachedFieldTrialParameter ACTION_BAR_VARIATION =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP, "action_bar", "");
    public static final StringCachedFieldTrialParameter THREE_BUTTON_ACTION_BAR_VARIATION =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR,
                    "three_button_action_bar", "");

    private static Boolean sItemBookmarkedForTesting;

    protected MenuItem mReloadMenuItem;

    protected final Context mContext;
    protected final boolean mIsTablet;
    protected final ActivityTabProvider mActivityTabProvider;
    protected final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    protected final TabModelSelector mTabModelSelector;
    protected final ToolbarManager mToolbarManager;
    protected final View mDecorView;
    private CallbackController mCallbackController = new CallbackController();
    private final ObservableSupplier<BookmarkBridge> mBookmarkBridgeSupplier;
    private Callback<BookmarkBridge> mBookmarkBridgeSupplierCallback;
    private boolean mUpdateMenuItemVisible;
    private ShareUtils mShareUtils;
    // Keeps track of which menu item was shown when installable app is detected.
    private int mAddAppTitleShown;
    private final ModalDialogManager mModalDialogManager;

    // The keys of the Map are menuitem ids, the first elements in the Pair are menuitem ids,
    // and the second elements in the Pair are AppMenuSimilarSelectionType. If users first
    // selected the menuitems in the Pair.first, and then selected a menuitem which is the key
    // if the Map, then users' selection match the pattern Pair.second.
    private static final Map<Integer, Pair<Set<Integer>, Integer>> sSimilarSelectedMenuItemMap =
            createSimilarSelectedMap();

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

    @IntDef({ThreeButtonActionBarType.DISABLED, ThreeButtonActionBarType.ACTION_CHIP_VIEW,
            ThreeButtonActionBarType.DESTINATION_CHIP_VIEW, ThreeButtonActionBarType.ADD_TO_OPTION})
    @interface ThreeButtonActionBarType {
        int DISABLED = 0;
        int ACTION_CHIP_VIEW = 1;
        int DESTINATION_CHIP_VIEW = 2;
        int ADD_TO_OPTION = 3;
    }

    /**
     * Keep this list sync with AppMenuSimilarSelectionType in enums.xml.
     */
    @IntDef({AppMenuSimilarSelectionType.NO_MATCH,
            AppMenuSimilarSelectionType.BOOKMARK_PAGE_THEN_ALL_BOOKMARKS,
            AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE,
            AppMenuSimilarSelectionType.DOWNLOAD_PAGE_THEN_ALL_DOWNLOADS,
            AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE})
    @interface AppMenuSimilarSelectionType {
        int NO_MATCH = -1;
        int BOOKMARK_PAGE_THEN_ALL_BOOKMARKS = 0;
        int ALL_BOOKMARKS_THEN_BOOKMARK_PAGE = 1;
        int DOWNLOAD_PAGE_THEN_ALL_DOWNLOADS = 2;
        int ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE = 3;
        int NUM_ENTRIES = 4;
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
     * @param modalDialogManager The {@link ModalDialogManager} that should be used to show "Add To"
     *         dialog.
     */
    public AppMenuPropertiesDelegateImpl(Context context, ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector, ToolbarManager toolbarManager, View decorView,
            @Nullable OneshotSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            ObservableSupplier<BookmarkBridge> bookmarkBridgeSupplier,
            ModalDialogManager modalDialogManager) {
        mContext = context;
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        mActivityTabProvider = activityTabProvider;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mTabModelSelector = tabModelSelector;
        mToolbarManager = toolbarManager;
        mDecorView = decorView;
        mModalDialogManager = modalDialogManager;

        if (overviewModeBehaviorSupplier != null) {
            overviewModeBehaviorSupplier.onAvailable(mCallbackController.makeCancelable(
                    overviewModeBehavior -> { mOverviewModeBehavior = overviewModeBehavior; }));
        }

        mBookmarkBridgeSupplier = bookmarkBridgeSupplier;
        mBookmarkBridgeSupplierCallback = (bookmarkBridge) -> mBookmarkBridge = bookmarkBridge;
        mBookmarkBridgeSupplier.addObserver(mBookmarkBridgeSupplierCallback);
        mShareUtils = new ShareUtils();
    }

    @Override
    public void destroy() {
        mBookmarkBridgeSupplier.removeObserver(mBookmarkBridgeSupplierCallback);
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    @Override
    public int getAppMenuLayoutId() {
        if (shouldShowRegroupedMenu() || shouldShowThreeButtonActionBar()) {
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
        customViewBinders.add(new ChipViewMenuItemViewBinder(getThreeButtonActionBarType()));
        customViewBinders.add(new AddToMenuItemViewBinder(mContext, mModalDialogManager));
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
            if (shouldShowThreeButtonActionBar()) {
                actionBar.removeItem(R.id.bookmark_this_page_id);
            } else {
                updateBookmarkMenuItem(bookmarkMenuItem, currentTab);
            }

            MenuItem offlineMenuItem = actionBar.findItem(R.id.offline_page_id);
            if (offlineMenuItem != null) {
                if (shouldShowThreeButtonActionBar()) {
                    actionBar.removeItem(R.id.offline_page_id);
                } else {
                    offlineMenuItem.setEnabled(shouldEnableDownloadPage(currentTab));
                }
            }

            MenuItem shareMenuItem = actionBar.findItem(R.id.share_menu_button_id);
            if (shareMenuItem != null) {
                if (shouldShowShareInMenu()) {
                    actionBar.removeItem(R.id.share_menu_button_id);
                } else {
                    shareMenuItem.setEnabled(mShareUtils.shouldEnableShare(currentTab));
                }
            }

            if (shouldShowInfoInMenu()) {
                actionBar.removeItem(R.id.info_menu_id);
            }

            if (shouldShowThreeButtonActionBar()) {
                assert actionBar.size() == 3;
            } else {
                assert actionBar.size() == 5;
            }
        }

        mUpdateMenuItemVisible = shouldShowUpdateMenuItem();
        menu.findItem(R.id.update_menu_id).setVisible(mUpdateMenuItemVisible);
        if (mUpdateMenuItemVisible) {
            mAppMenuInvalidator = () -> handler.invalidateAppMenu();
            UpdateMenuItemHelper.getInstance().registerObserver(mAppMenuInvalidator);
        }

        menu.findItem(R.id.move_to_other_window_menu_id).setVisible(shouldShowMoveToOtherWindow());

        @ThreeButtonActionBarType
        int threeButtonActionBarType = getThreeButtonActionBarType();
        boolean addToOptionVisible =
                threeButtonActionBarType == ThreeButtonActionBarType.ADD_TO_OPTION;
        MenuItem addToDividerLineItem = menu.findItem(R.id.add_to_divider_line_id);
        if (addToDividerLineItem != null) {
            addToDividerLineItem.setVisible(addToOptionVisible);
            addToDividerLineItem.setEnabled(false);
        }
        // Duplicating add_to_homescreen/install_app/open_webapk is for
        // the purpose of experiment,  one of them will be removed once the
        // experiments are done.
        MenuItem addToMenuItem = menu.findItem(R.id.add_to_menu_id);
        if (addToMenuItem != null) {
            addToMenuItem.setVisible(addToOptionVisible);
        }
        MenuItem installAppItem = menu.findItem(R.id.install_app_id);
        if (installAppItem != null) {
            // Visible will be changed later by #prepareAddToHomescreenMenuItem.
            installAppItem.setVisible(addToOptionVisible);
        }
        MenuItem menuOpenWebApkItem = menu.findItem(R.id.menu_open_webapk_id);
        if (menuOpenWebApkItem != null) {
            // Visible will be changed later by #prepareAddToHomescreenMenuItem.
            menuOpenWebApkItem.setVisible(addToOptionVisible);
        }

        if (shouldShowThreeButtonActionBar()) {
            MenuItem downloadMenuItem =
                    menu.findItem(R.id.downloads_row_menu_id).getSubMenu().getItem(1);
            assert downloadMenuItem.getItemId() == R.id.offline_page_chip_id;
            downloadMenuItem.setEnabled(shouldEnableDownloadPage(currentTab));

            MenuItem bookmarkMenuItem =
                    menu.findItem(R.id.all_bookmarks_row_menu_id).getSubMenu().getItem(1);
            assert bookmarkMenuItem.getItemId() == R.id.bookmark_this_page_chip_id;
            updateBookmarkMenuItem(bookmarkMenuItem, currentTab);

            // Update titles for ChipView menu items.
            if (threeButtonActionBarType == ThreeButtonActionBarType.ACTION_CHIP_VIEW) {
                downloadMenuItem.setTitle(R.string.add);
                if (bookmarkMenuItem.isChecked()) {
                    bookmarkMenuItem.setTitle(R.string.bookmark_item_edit);
                } else {
                    bookmarkMenuItem.setTitle(R.string.add);
                }
            } else if (threeButtonActionBarType == ThreeButtonActionBarType.DESTINATION_CHIP_VIEW) {
                MenuItem allDownloadMenuItem =
                        menu.findItem(R.id.downloads_row_menu_id).getSubMenu().getItem(0);
                assert allDownloadMenuItem.getItemId() == R.id.downloads_menu_id;
                allDownloadMenuItem.setTitle(R.string.all);

                MenuItem allBookmarkMenuItem =
                        menu.findItem(R.id.all_bookmarks_row_menu_id).getSubMenu().getItem(0);
                assert allBookmarkMenuItem.getItemId() == R.id.all_bookmarks_menu_id;
                allBookmarkMenuItem.setTitle(R.string.all);
            } else if (threeButtonActionBarType == ThreeButtonActionBarType.ADD_TO_OPTION) {
                MenuItem addToBookmarksMenuItem =
                        addToMenuItem.getSubMenu().findItem(R.id.add_to_bookmarks_menu_id);
                updateBookmarkMenuItem(addToBookmarksMenuItem, currentTab);

                MenuItem addToReadingListMenuItem =
                        addToMenuItem.getSubMenu().findItem(R.id.add_to_reading_list_menu_id);
                addToReadingListMenuItem.setVisible(
                        CachedFeatureFlags.isEnabled(ChromeFeatureList.READ_LATER));
                addToReadingListMenuItem.setEnabled(ReadingListUtils.isReadingListSupported(url));

                MenuItem addToDownloadsMenuItem =
                        addToMenuItem.getSubMenu().findItem(R.id.add_to_downloads_menu_id);
                addToDownloadsMenuItem.setEnabled(shouldEnableDownloadPage(currentTab));

                MenuItem addToHomescreenMenuItem =
                        addToMenuItem.getSubMenu().findItem(R.id.add_to_homescreen_menu_id);
                prepareAddToHomescreenMenuItem(addToHomescreenMenuItem, installAppItem,
                        menuOpenWebApkItem, menu, currentTab,
                        shouldShowHomeScreenMenuItem(
                                isChromeScheme, isFileScheme, isContentScheme, isIncognito, url));
                if (addToHomescreenMenuItem.isVisible()) {
                    // addToHomescreenMenuItem in "Add to" dialog uses a different string.
                    addToHomescreenMenuItem.setTitle(R.string.menu_homescreen);
                }
            }
        }

        // Don't allow either "chrome://" pages or interstitial pages to be shared.
        menu.findItem(R.id.share_row_menu_id)
                .setVisible(mShareUtils.shouldEnableShare(currentTab) && shouldShowShareInMenu());

        ShareHelper.configureDirectShareMenuItem(
                mContext, menu.findItem(R.id.direct_share_menu_id));

        menu.findItem(R.id.paint_preview_show_id)
                .setVisible(shouldShowPaintPreview(isChromeScheme, currentTab, isIncognito));

        // Enable image descriptions if the feature flag is enabled, and if a screen reader
        // is currently running.
        if (ImageDescriptionsController.getInstance().shouldShowImageDescriptionsMenuItem()) {
            menu.findItem(R.id.get_image_descriptions_id).setVisible(true);

            int titleId = R.string.menu_stop_image_descriptions;
            Profile profile = Profile.fromWebContents(currentTab.getWebContents());
            // If image descriptions are not enabled, then we want the menu item to be "Get".
            if (!ImageDescriptionsController.getInstance().imageDescriptionsEnabled(profile)) {
                titleId = R.string.menu_get_image_descriptions;
            } else if (ImageDescriptionsController.getInstance().onlyOnWifiEnabled(profile)
                    && DeviceConditions.getCurrentNetConnectionType(mContext)
                            != ConnectionType.CONNECTION_WIFI) {
                // If image descriptions are enabled, then we want "Stop", except in the special
                // case that the user specified only on Wifi, and we are not currently on Wifi.
                titleId = R.string.menu_get_image_descriptions;
            }

            menu.findItem(R.id.get_image_descriptions_id).setTitle(titleId);
        } else {
            menu.findItem(R.id.get_image_descriptions_id).setVisible(false);
        }

        // Disable find in page on the native NTP.
        menu.findItem(R.id.find_in_page_id).setVisible(shouldShowFindInPage(currentTab));

        // Prepare translate menu button.
        prepareTranslateMenuItem(menu, currentTab);

        MenuItem homescreenItem = menu.findItem(R.id.add_to_homescreen_id);
        MenuItem openWebApkItem = menu.findItem(R.id.open_webapk_id);
        if (addToOptionVisible) {
            homescreenItem.setVisible(false);
            openWebApkItem.setVisible(false);
        } else {
            prepareAddToHomescreenMenuItem(homescreenItem, null, openWebApkItem, menu, currentTab,
                    shouldShowHomeScreenMenuItem(
                            isChromeScheme, isFileScheme, isContentScheme, isIncognito, url));
        }

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
        boolean isPriceTrackingVisible = PriceTrackingUtilities.isPriceTrackingEligible()
                && !DeviceClassManager.enableAccessibilityLayout() && !isIncognito;
        boolean isPriceTrackingEnabled = isPriceTrackingVisible;

        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (!shouldShowIconBeforeItem()) {
                // Remove icons for menu items except the reader mode prefs and the update menu
                // item.
                if (item.getItemId() != R.id.reader_mode_prefs_id
                        && item.getItemId() != R.id.update_menu_id) {
                    item.setIcon(null);
                }
                // Remove icons for menu items that have submenus.
                if (item.getItemId() == R.id.downloads_row_menu_id
                        || item.getItemId() == R.id.all_bookmarks_row_menu_id
                        || item.getItemId() == R.id.add_to_menu_id) {
                    for (int j = 0; j < item.getSubMenu().size(); ++j) {
                        item.getSubMenu().getItem(j).setIcon(null);
                    }
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
            if (item.getItemId() == R.id.track_prices_row_menu_id) {
                item.setVisible(isPriceTrackingVisible);
                item.setEnabled(isPriceTrackingEnabled);
                if (isPriceTrackingVisible) {
                    menu.findItem(R.id.track_prices_check_id)
                            .setChecked(PriceTrackingUtilities.isTrackPricesOnTabsEnabled());
                }
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
     * This method should only be called once per context menu shown.
     * @param currentTab The currentTab for which the app menu is showing.
     * @param logging Whether logging should be performed in this check.
     * @return Whether the translate menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowTranslateMenuItem(@NonNull Tab currentTab) {
        return TranslateUtils.canTranslateCurrentTab(currentTab, true);
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
        return WebappsUtils.isAddToHomeIntentSupported() && !isChromeScheme && !isFileScheme
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
    protected void prepareAddToHomescreenMenuItem(MenuItem homescreenItem,
            @Nullable MenuItem installAppItem, MenuItem openWebApkItem, Menu menu, Tab currentTab,
            boolean shouldShowHomeScreenMenuItem) {
        mAddAppTitleShown = AppMenuVerbiage.APP_MENU_OPTION_UNKNOWN;
        if (!shouldShowHomeScreenMenuItem) {
            homescreenItem.setVisible(false);
            openWebApkItem.setVisible(false);
            if (installAppItem != null) {
                installAppItem.setVisible(false);
            }
            return;
        }

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
            if (installAppItem != null) {
                installAppItem.setVisible(false);
            }
        } else {
            AppBannerManager.InstallStringPair installStrings = getAddToHomeScreenTitle(currentTab);
            // When "Add to" mernu item is enabled for the app menu, if the current webpage is a PWA
            // then the menu item to "Install app" ({@code installAppItem}) will be shown in the
            // main menu. If the current webpage is not a PWA "Add to homescreen" will be shown in
            // the "Add to dialog" instead. If {@code installAppItem} is not null, ensure that only
            // one of installAppItem or homescreenItem are visible.
            if (installAppItem != null
                    && installStrings.titleTextId == AppBannerManager.PWA_PAIR.titleTextId) {
                installAppItem.setTitle(installStrings.titleTextId);
                installAppItem.setVisible(true);
                homescreenItem.setVisible(false);
            } else {
                homescreenItem.setTitle(installStrings.titleTextId);
                homescreenItem.setVisible(true);
                if (installAppItem != null) {
                    installAppItem.setVisible(false);
                }
            }
            openWebApkItem.setVisible(false);

            if (installStrings.titleTextId == AppBannerManager.NON_PWA_PAIR.titleTextId) {
                mAddAppTitleShown = AppMenuVerbiage.APP_MENU_OPTION_ADD_TO_HOMESCREEN;
            } else if (installStrings.titleTextId == AppBannerManager.PWA_PAIR.titleTextId) {
                mAddAppTitleShown = AppMenuVerbiage.APP_MENU_OPTION_INSTALL;
            }
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public AppBannerManager.InstallStringPair getAddToHomeScreenTitle(@NonNull Tab currentTab) {
        return AppBannerManager.getHomescreenLanguageOption(currentTab.getWebContents());
    }

    @Override
    public Bundle getBundleForMenuItem(MenuItem item) {
        Bundle bundle = new Bundle();
        if (item.getItemId() == R.id.add_to_homescreen_id
                || item.getItemId() == R.id.add_to_homescreen_menu_id
                || item.getItemId() == R.id.install_app_id) {
            bundle.putInt(AppBannerManager.MENU_TITLE_KEY, mAddAppTitleShown);
        }
        return bundle;
    }

    /**
     * Sets the visibility of the "Translate" menu item.
     */
    protected void prepareTranslateMenuItem(Menu menu, Tab currentTab) {
        boolean isTranslateVisible = shouldShowTranslateMenuItem(currentTab);
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

        final boolean isMenuButtonOnTop = mToolbarManager != null;
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

    private boolean shouldShowRegroupedMenu() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_REGROUP);
    }

    private static boolean shouldShowThreeButtonActionBar() {
        return CachedFeatureFlags.isEnabled(
                ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR);
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
        if (shouldShowRegroupedMenu()) {
            if (ACTION_BAR_VARIATION.getValue().equals("backward_button")) {
                return ActionBarType.BACKWARD_BUTTON;
            } else if (ACTION_BAR_VARIATION.getValue().equals("share_button")) {
                return ActionBarType.SHARE_BUTTON;
            }
        }
        return ActionBarType.STANDARD;
    }

    /**
     * @return The type of three button action bar should be shown.
     */
    private static @ThreeButtonActionBarType int getThreeButtonActionBarType() {
        if (shouldShowThreeButtonActionBar()) {
            if (THREE_BUTTON_ACTION_BAR_VARIATION.getValue().equals("action_chip_view")) {
                return ThreeButtonActionBarType.ACTION_CHIP_VIEW;
            } else if (THREE_BUTTON_ACTION_BAR_VARIATION.getValue().equals(
                               "destination_chip_view")) {
                return ThreeButtonActionBarType.DESTINATION_CHIP_VIEW;
            } else if (THREE_BUTTON_ACTION_BAR_VARIATION.getValue().equals("add_to_option")) {
                return ThreeButtonActionBarType.ADD_TO_OPTION;
            }
        }
        return ThreeButtonActionBarType.DISABLED;
    }

    /**
     * @return The "download" menu items id in the app menu.
     */
    public static int getOfflinePageId() {
        @ThreeButtonActionBarType
        int type = getThreeButtonActionBarType();
        if (type == ThreeButtonActionBarType.ACTION_CHIP_VIEW
                || type == ThreeButtonActionBarType.DESTINATION_CHIP_VIEW) {
            return R.id.offline_page_chip_id;
        } else if (type == ThreeButtonActionBarType.ADD_TO_OPTION) {
            return R.id.add_to_downloads_menu_id;
        }
        return R.id.offline_page_id;
    }

    /**
     * @return The "Add to Home screen" menu items id in the app menu.
     */
    public static int getAddToHomescreenId() {
         if (getThreeButtonActionBarType() == ThreeButtonActionBarType.ADD_TO_OPTION) {
            return R.id.add_to_homescreen_menu_id;
        }
        return R.id.add_to_homescreen_id;
    }

    @Override
    public boolean recordAppMenuSimilarSelectionIfNeeded(
            int previousMenuItemId, int currentMenuItemId) {
        @AppMenuSimilarSelectionType
        int pattern = findSimilarSelectionPattern(previousMenuItemId, currentMenuItemId);
        if (pattern == AppMenuSimilarSelectionType.NO_MATCH) {
            return false;
        }

        RecordHistogram.recordEnumeratedHistogram("Mobile.AppMenu.SimilarSelection", pattern,
                AppMenuSimilarSelectionType.NUM_ENTRIES);
        return true;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public @AppMenuSimilarSelectionType int findSimilarSelectionPattern(
            int previousMenuItemId, int currentMenuItemId) {
        Pair<Set<Integer>, Integer> menuItemToSelectType =
                sSimilarSelectedMenuItemMap.get(currentMenuItemId);
        if (menuItemToSelectType != null
                && menuItemToSelectType.first.contains(previousMenuItemId)) {
            return menuItemToSelectType.second;
        }

        return AppMenuSimilarSelectionType.NO_MATCH;
    }

    private static Map<Integer, Pair<Set<Integer>, Integer>> createSimilarSelectedMap() {
        Map<Integer, Pair<Set<Integer>, Integer>> map = new LinkedHashMap<>();
        map.put(R.id.all_bookmarks_menu_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.bookmark_this_page_id,
                                R.id.bookmark_this_page_chip_id, R.id.add_to_bookmarks_menu_id)),
                        AppMenuSimilarSelectionType.BOOKMARK_PAGE_THEN_ALL_BOOKMARKS));
        map.put(R.id.bookmark_this_page_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.all_bookmarks_menu_id)),
                        AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE));
        map.put(R.id.bookmark_this_page_chip_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.all_bookmarks_menu_id)),
                        AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE));
        map.put(R.id.add_to_bookmarks_menu_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.all_bookmarks_menu_id)),
                        AppMenuSimilarSelectionType.ALL_BOOKMARKS_THEN_BOOKMARK_PAGE));
        map.put(R.id.downloads_menu_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.offline_page_id, R.id.offline_page_chip_id,
                                R.id.add_to_downloads_menu_id)),
                        AppMenuSimilarSelectionType.DOWNLOAD_PAGE_THEN_ALL_DOWNLOADS));
        map.put(R.id.offline_page_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.downloads_menu_id)),
                        AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE));
        map.put(R.id.offline_page_chip_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.downloads_menu_id)),
                        AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE));
        map.put(R.id.add_to_downloads_menu_id,
                new Pair<Set<Integer>, Integer>(
                        new HashSet<>(Arrays.asList(R.id.downloads_menu_id)),
                        AppMenuSimilarSelectionType.ALL_DOWNLOADS_THEN_DOWNLOAD_PAGE));
        return map;
    }
}
