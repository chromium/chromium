// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.SparseArray;
import android.view.Menu;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ai.AiAssistantService;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.app.creator.CreatorActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feed.webfeed.WebFeedFaviconFetcher;
import org.chromium.chrome.browser.feed.webfeed.WebFeedMainMenuItem;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSnackbarController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tinker_tank.TinkerTankDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Function;

/** An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity. */
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    @IntDef({TabbedAppMenuItemType.UPDATE_ITEM, TabbedAppMenuItemType.NEW_INCOGNITO_TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabbedAppMenuItemType {
        /** Regular Android menu item that contains a title and an icon if icon is specified. */
        int UPDATE_ITEM = AppMenuHandler.AppMenuItemType.NUM_ENTRIES;

        /**
         * Menu item that has two buttons, the first one is a title and the second one is an icon.
         * It is different from the regular menu item because it contains two separate buttons.
         */
        int NEW_INCOGNITO_TAB = AppMenuHandler.AppMenuItemType.NUM_ENTRIES + 1;
    }

    AppMenuDelegate mAppMenuDelegate;
    WebFeedSnackbarController.FeedLauncher mFeedLauncher;
    ModalDialogManager mModalDialogManager;
    SnackbarManager mSnackbarManager;

    private boolean mUpdateMenuItemVisible;

    /**
     * This is non null for the case of ChromeTabbedActivity when the corresponding {@link
     * CallbackController} has been fired.
     */
    private @Nullable IncognitoReauthController mIncognitoReauthController;

    private final CallbackController mIncognitoReauthCallbackController = new CallbackController();

    public TabbedAppMenuPropertiesDelegate(
            Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager,
            View decorView,
            AppMenuDelegate appMenuDelegate,
            OneshotSupplier<LayoutStateProvider> layoutStateProvider,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            WebFeedSnackbarController.FeedLauncher feedLauncher,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            @NonNull
                    OneshotSupplier<IncognitoReauthController>
                            incognitoReauthControllerOneshotSupplier,
            Supplier<ReadAloudController> readAloudControllerSupplier) {
        super(
                context,
                activityTabProvider,
                multiWindowModeStateDispatcher,
                tabModelSelector,
                toolbarManager,
                decorView,
                layoutStateProvider,
                bookmarkModelSupplier,
                readAloudControllerSupplier);
        mAppMenuDelegate = appMenuDelegate;
        mFeedLauncher = feedLauncher;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;

        incognitoReauthControllerOneshotSupplier.onAvailable(
                mIncognitoReauthCallbackController.makeCancelable(
                        incognitoReauthController -> {
                            mIncognitoReauthController = incognitoReauthController;
                        }));
    }

    @Override
    public void registerCustomViewBinders(
            ModelListAdapter modelListAdapter,
            SparseArray<Function<Context, Integer>> customSizingSuppliers) {
        modelListAdapter.registerType(
                TabbedAppMenuItemType.UPDATE_ITEM,
                new LayoutViewBuilder(R.layout.update_menu_item),
                UpdateMenuItemViewBinder::bind);
        customSizingSuppliers.append(
                TabbedAppMenuItemType.UPDATE_ITEM, UpdateMenuItemViewBinder::getPixelHeight);

        modelListAdapter.registerType(
                TabbedAppMenuItemType.NEW_INCOGNITO_TAB,
                new LayoutViewBuilder(R.layout.custom_view_menu_item),
                IncognitoMenuItemViewBinder::bind);
    }

    @Override
    public void prepareMenu(Menu menu, AppMenuHandler handler) {
        int menuGroup = getMenuGroup();
        setMenuGroupVisibility(menuGroup, menu);

        boolean isIncognito = mTabModelSelector.getCurrentModel().isIncognito();
        Tab currentTab = mActivityTabProvider.get();

        if (menuGroup == MenuGroup.PAGE_MENU) {
            preparePageMenu(menu, currentTab, handler, isIncognito);
        }
        prepareCommonMenuItems(menu, menuGroup, isIncognito);
    }

    private void setMenuGroupVisibility(@MenuGroup int menuGroup, Menu menu) {
        menu.setGroupVisible(R.id.PAGE_MENU, menuGroup == MenuGroup.PAGE_MENU);
        menu.setGroupVisible(R.id.OVERVIEW_MODE_MENU, menuGroup == MenuGroup.OVERVIEW_MODE_MENU);
        menu.setGroupVisible(
                R.id.TABLET_EMPTY_MODE_MENU, menuGroup == MenuGroup.TABLET_EMPTY_MODE_MENU);
    }

    /** Prepare the menu items. Note: it is possible that currentTab is null. */
    private void preparePageMenu(
            Menu menu, @Nullable Tab currentTab, AppMenuHandler handler, boolean isIncognito) {
        // Multiple menu items shouldn't be enabled when the currentTab is null. Use a flag to
        // indicate whether the current Tab isn't null.
        boolean isCurrentTabNotNull = currentTab != null;

        GURL url = isCurrentTabNotNull ? currentTab.getUrl() : GURL.emptyGURL();
        final boolean isNativePage =
                url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                        || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)
                        || (isCurrentTabNotNull && currentTab.isNativePage());
        final boolean isFileScheme = url.getScheme().equals(UrlConstants.FILE_SCHEME);
        final boolean isContentScheme = url.getScheme().equals(UrlConstants.CONTENT_SCHEME);

        // Update the icon row items (shown in narrow form factors).
        boolean shouldShowIconRow = shouldShowIconRow();
        menu.findItem(R.id.icon_row_menu_id).setVisible(shouldShowIconRow);
        if (shouldShowIconRow) {
            SubMenu actionBar = menu.findItem(R.id.icon_row_menu_id).getSubMenu();

            // Disable the "Forward" menu item if there is no page to go to.
            MenuItem forwardMenuItem = actionBar.findItem(R.id.forward_menu_id);
            forwardMenuItem.setEnabled(isCurrentTabNotNull && currentTab.canGoForward());

            Drawable icon = AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
            DrawableCompat.setTintList(
                    icon,
                    AppCompatResources.getColorStateList(
                            mContext, R.color.default_icon_color_tint_list));
            actionBar.findItem(R.id.reload_menu_id).setIcon(icon);
            loadingStateChanged(isCurrentTabNotNull && currentTab.isLoading());

            MenuItem bookmarkMenuItemShortcut = actionBar.findItem(R.id.bookmark_this_page_id);
            updateBookmarkMenuItemShortcut(
                    bookmarkMenuItemShortcut, currentTab, /* fromCct= */ false);

            MenuItem offlineMenuItem = actionBar.findItem(R.id.offline_page_id);
            offlineMenuItem.setEnabled(isCurrentTabNotNull && shouldEnableDownloadPage(currentTab));

            if (!isCurrentTabNotNull) {
                actionBar.findItem(R.id.info_menu_id).setEnabled(false);
                actionBar.findItem(R.id.reload_menu_id).setEnabled(false);
            }
            assert actionBar.size() == 5;
        }

        mUpdateMenuItemVisible = shouldShowUpdateMenuItem();
        menu.findItem(R.id.update_menu_id).setVisible(mUpdateMenuItemVisible);
        if (mUpdateMenuItemVisible) {
            mAppMenuInvalidator = () -> handler.invalidateAppMenu();
            UpdateMenuItemHelper.getInstance(mTabModelSelector.getModel(false).getProfile())
                    .registerObserver(mAppMenuInvalidator);
        }

        menu.findItem(R.id.new_window_menu_id).setVisible(shouldShowNewWindow());
        menu.findItem(R.id.move_to_other_window_menu_id).setVisible(shouldShowMoveToOtherWindow());
        MenuItem menu_all_windows = menu.findItem(R.id.manage_all_windows_menu_id);
        boolean showManageAllWindows = MultiWindowUtils.shouldShowManageWindowsMenu();
        menu_all_windows.setVisible(showManageAllWindows);
        if (showManageAllWindows) {
            menu_all_windows.setTitle(
                    mContext.getString(R.string.menu_manage_all_windows, getInstanceCount()));
        }

        updatePriceTrackingMenuItemRow(
                menu.findItem(R.id.enable_price_tracking_menu_id),
                menu.findItem(R.id.disable_price_tracking_menu_id),
                currentTab);

        updateAiMenuItemRow(
                menu.findItem(R.id.ai_web_menu_id), menu.findItem(R.id.ai_pdf_menu_id), currentTab);

        boolean showOpenWith =
                currentTab != null
                        && currentTab.isNativePage()
                        && currentTab.getNativePage().isPdf();
        menu.findItem(R.id.open_with_id).setVisible(showOpenWith);

        // Don't allow either "chrome://" pages or interstitial pages to be shared, or when the
        // current tab is null.
        boolean showShare = isCurrentTabNotNull && ShareUtils.shouldEnableShare(currentTab);
        menu.findItem(R.id.share_row_menu_id).setVisible(showShare);

        if (isCurrentTabNotNull) {
            updateDirectShareMenuItem(menu.findItem(R.id.direct_share_menu_id));
        }

        // For the non-desktop case, Print action will be showed in the Share UI instead.
        boolean showPrint =
                showShare
                        && BuildConfig.IS_DESKTOP_ANDROID
                        && UserPrefs.get(currentTab.getProfile()).getBoolean(Pref.PRINTING_ENABLED);
        menu.findItem(R.id.print_id).setVisible(showPrint);

        menu.findItem(R.id.paint_preview_show_id)
                .setVisible(
                        isCurrentTabNotNull
                                && shouldShowPaintPreview(isNativePage, currentTab, isIncognito));

        menu.findItem(R.id.add_to_group_menu_id)
                .setVisible(ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled())
                .setTitle(
                        getAddToGroupMenuItemString(
                                currentTab != null ? currentTab.getTabGroupId() : null));

        // Enable image descriptions if touch exploration is currently enabled, but not on the
        // native NTP.
        if (isCurrentTabNotNull
                && shouldShowWebContentsDependentMenuItem(currentTab)
                && ImageDescriptionsController.getInstance()
                        .shouldShowImageDescriptionsMenuItem()) {
            menu.findItem(R.id.get_image_descriptions_id).setVisible(true);

            int titleId = R.string.menu_stop_image_descriptions;
            Profile profile = currentTab.getProfile();
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

        // Conditionally add the Zoom menu item, but not on the native NTP.
        menu.findItem(R.id.page_zoom_id)
                .setVisible(
                        isCurrentTabNotNull
                                && shouldShowWebContentsDependentMenuItem(currentTab)
                                && PageZoomCoordinator.shouldShowMenuItem());

        // Disable find in page on the native NTP (except for PDF native page).
        updateFindInPageMenuItem(menu, currentTab);

        // Prepare translate menu button.
        prepareTranslateMenuItem(menu, currentTab);

        // Set visibility of Read Aloud menu item.
        prepareReadAloudMenuItem(menu, currentTab);

        prepareAddToHomescreenMenuItem(
                menu,
                currentTab,
                shouldShowHomeScreenMenuItem(
                        isNativePage, isFileScheme, isContentScheme, isIncognito, url));

        updateRequestDesktopSiteMenuItem(menu, currentTab, true /* can show */, isNativePage);

        updateAutoDarkMenuItem(menu, currentTab, isNativePage);

        // Only display reader mode settings menu option if the current page is in reader mode.
        menu.findItem(R.id.reader_mode_prefs_id)
                .setVisible(isCurrentTabNotNull && shouldShowReaderModePrefs(currentTab));
        menu.findItem(R.id.reader_mode_menu_id)
                .setVisible(DomDistillerFeatures.showAlwaysOnEntryPoint());

        updateManagedByMenuItem(menu, currentTab);

        // Only display quick delete divider line on the regular mode page menu.
        menu.findItem(R.id.quick_delete_divider_line_id).setVisible(!isIncognito);

        menu.findItem(R.id.download_page_id).setVisible(shouldShowDownloadPageMenuItem(currentTab));

        menu.findItem(R.id.ntp_customization_id)
                .setVisible(shouldShowNtpCustomizations(currentTab, isIncognito));
    }

    /**
     * Returns True if the NTP Customization menu entry should be visible.
     *
     * <p>This entry is shown only when the corresponding feature flag is enabled and the user is on
     * the regular Ntp.
     */
    private boolean shouldShowNtpCustomizations(@Nullable Tab currentTab, boolean isIncognito) {
        return ChromeFeatureList.sNewTabPageCustomization.isEnabled()
                && !isIncognito
                && currentTab != null
                && UrlUtilities.isNtpUrl(currentTab.getUrl());
    }

    private void prepareCommonMenuItems(Menu menu, @MenuGroup int menuGroup, boolean isIncognito) {
        // We have to iterate all menu items since same menu item ID may be associated with more
        // than one menu items.
        boolean isOverviewModeMenu = menuGroup == MenuGroup.OVERVIEW_MODE_MENU;

        // Disable incognito group and select tabs when a re-authentication screen is shown.
        // We show the re-auth screen only in Incognito mode.
        boolean isIncognitoReauthShowing =
                isIncognito
                        && (mIncognitoReauthController != null)
                        && mIncognitoReauthController.isReauthPageShowing();

        boolean isMenuSelectTabsVisible = isOverviewModeMenu;
        boolean isMenuSelectTabsEnabled =
                !isIncognitoReauthShowing
                        && isMenuSelectTabsVisible
                        && mTabModelSelector.isTabStateInitialized()
                        && mTabModelSelector.getModel(isIncognito).getCount() != 0;

        boolean hasItemBetweenDividers = false;

        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (!shouldShowIconBeforeItem()) {
                // Remove icons for menu items except the reader mode prefs and the update menu
                // item.
                if (item.getItemId() != R.id.reader_mode_prefs_id
                        && item.getItemId() != R.id.update_menu_id) {
                    item.setIcon(null);
                }

                // Remove title button icons.
                if (item.getItemId() == R.id.request_desktop_site_row_menu_id
                        || item.getItemId() == R.id.share_row_menu_id
                        || item.getItemId() == R.id.auto_dark_web_contents_row_menu_id) {
                    item.getSubMenu().getItem(0).setIcon(null);
                }
            }

            if (item.getItemId() == R.id.new_incognito_tab_menu_id && item.isVisible()) {
                // Disable new incognito tab when it is blocked (e.g. by a policy).
                // findItem(...).setEnabled(...)" is not enough here, because of the inflated
                // main_menu.xml contains multiple items with the same id in different groups
                // e.g.: menu_new_incognito_tab.
                // Disable new incognito tab when a re-authentication might be showing.
                item.setEnabled(isIncognitoEnabled() && !isIncognitoReauthShowing);
            }

            if (item.getItemId() == R.id.divider_line_id) {
                item.setEnabled(false);
            }

            int itemGroupId = item.getGroupId();
            if (!((menuGroup == MenuGroup.OVERVIEW_MODE_MENU
                            && itemGroupId == R.id.OVERVIEW_MODE_MENU)
                    || (menuGroup == MenuGroup.PAGE_MENU && itemGroupId == R.id.PAGE_MENU))) {
                continue;
            }

            if (item.getItemId() == R.id.recent_tabs_menu_id) {
                item.setVisible(!isIncognito);
            }
            if (item.getItemId() == R.id.menu_select_tabs) {
                item.setVisible(isMenuSelectTabsVisible);
                item.setEnabled(isMenuSelectTabsEnabled);
            }
            if (item.getItemId() == R.id.close_all_tabs_menu_id) {
                boolean hasTabs = mTabModelSelector.getTotalTabCount() > 0;
                item.setVisible(!isIncognito && isOverviewModeMenu);
                item.setEnabled(hasTabs);
            }
            if (item.getItemId() == R.id.close_all_incognito_tabs_menu_id) {
                boolean hasIncognitoTabs = mTabModelSelector.getModel(true).getCount() > 0;
                item.setVisible(isIncognito && isOverviewModeMenu);
                item.setEnabled(hasIncognitoTabs);
            }
            if (item.getItemId() == R.id.new_tab_group_menu_id) {
                item.setVisible(ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled());
            }
            if (item.getItemId() == R.id.quick_delete_menu_id) {
                item.setVisible(!isIncognito);
            }
            if (item.getItemId() == R.id.tinker_tank_menu_id) {
                boolean enabled = TinkerTankDelegate.isEnabled();
                item.setVisible(enabled);
                item.setEnabled(enabled);
            }

            // This needs to be done after the visibility of the item is set.
            if (item.getItemId() == R.id.divider_line_id) {
                if (!hasItemBetweenDividers) {
                    // If there isn't any visible menu items between the two divider lines, mark
                    // this line invisible.
                    item.setVisible(false);
                } else {
                    hasItemBetweenDividers = false;
                }
            } else if (!hasItemBetweenDividers && item.isVisible()) {
                // When the item isn't a divider line and is visible, we set hasItemBetweenDividers
                // to be true.
                hasItemBetweenDividers = true;
            }
        }
    }

    @Override
    protected int getItemTypeForId(int menuItemId) {
        if (menuItemId == R.id.new_incognito_tab_menu_id) {
            return TabbedAppMenuItemType.NEW_INCOGNITO_TAB;
        } else if (menuItemId == R.id.update_menu_id) {
            return TabbedAppMenuItemType.UPDATE_ITEM;
        }

        return super.getItemTypeForId(menuItemId);
    }

    @Override
    protected MVCListAdapter.ListItem buildPropertyModelForMenuItem(MenuItem item) {
        MVCListAdapter.ListItem listItem = super.buildPropertyModelForMenuItem(item);
        if (listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == R.id.update_menu_id) {
            listItem.model.set(
                    AppMenuItemProperties.CUSTOM_ITEM_DATA,
                    UpdateMenuItemHelper.getInstance(mTabModelSelector.getModel(false).getProfile())
                            .getUiState()
                            .itemState);
        }
        return listItem;
    }

    @VisibleForTesting
    boolean shouldShowIconRow() {
        boolean shouldShowIconRow =
                mIsTablet
                        ? mDecorView.getWidth()
                                < DeviceFormFactor.getNonMultiDisplayMinimumTabletWidthPx(mContext)
                        : true;

        final boolean isMenuButtonOnTop = mToolbarManager != null;
        shouldShowIconRow &= isMenuButtonOnTop;
        return shouldShowIconRow;
    }

    /**
     * Updates the find in page menu item's state.
     *
     * @param menu {@link Menu} for find in page.
     * @param currentTab Current tab being displayed.
     */
    private void updateFindInPageMenuItem(Menu menu, @Nullable Tab currentTab) {
        MenuItem findInPageMenuRow = menu.findItem(R.id.find_in_page_id);
        // PDF native page should show find in page menu item.
        boolean itemVisible =
                currentTab != null
                        && (shouldShowWebContentsDependentMenuItem(currentTab)
                                || (currentTab.isNativePage()
                                        && currentTab.getNativePage().isPdf()));
        findInPageMenuRow.setVisible(itemVisible);
    }

    private void updateAiMenuItemRow(
            @NonNull MenuItem aiWebMenuItem,
            @NonNull MenuItem aiPdfMenuItem,
            @Nullable Tab currentTab) {
        if (currentTab == null
                || currentTab.getWebContents() == null
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
                || !AiAssistantService.getInstance().canShowAiForTab(mContext, currentTab)) {
            aiWebMenuItem.setVisible(false);
            aiPdfMenuItem.setVisible(false);
            return;
        }

        if (currentTab.isNativePage() && currentTab.getNativePage() instanceof PdfPage) {
            aiWebMenuItem.setVisible(false);
            aiPdfMenuItem.setVisible(true);
        } else if (currentTab.getUrl() != null && UrlUtilities.isHttpOrHttps(currentTab.getUrl())) {
            aiWebMenuItem.setVisible(true);
            aiPdfMenuItem.setVisible(false);
        } else {
            aiWebMenuItem.setVisible(false);
            aiPdfMenuItem.setVisible(false);
        }
    }

    /**
     * @param isNativePage Whether the current tab is a native page.
     * @param currentTab The currentTab for which the app menu is showing.
     * @param isIncognito Whether the currentTab is incognito.
     * @return Whether the paint preview menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowPaintPreview(
            boolean isNativePage, @NonNull Tab currentTab, boolean isIncognito) {
        return ChromeFeatureList.sPaintPreviewDemo.isEnabled() && !isNativePage && !isIncognito;
    }

    /**
     * @return Whether the "New window" menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowNewWindow() {
        // Hide the menu on automotive devices.
        if (BuildInfo.getInstance().isAutomotive) return false;

        if (instanceSwitcherWithMultiInstanceEnabled()) {
            // Hide the menu if we already have the maximum number of windows.
            if (getInstanceCount() >= MultiWindowUtils.getMaxInstances()) return false;

            // On phones, show the menu only when in split-screen, with a single instance
            // running on the foreground.
            return isTabletSizeScreen()
                    || (!mMultiWindowModeStateDispatcher.isChromeRunningInAdjacentWindow()
                            && (mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                                    || mMultiWindowModeStateDispatcher.isInMultiDisplayMode()));
        } else {
            if (mMultiWindowModeStateDispatcher.isMultiInstanceRunning()) return false;
            return (mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()
                            && isTabletSizeScreen())
                    || mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                    || mMultiWindowModeStateDispatcher.isInMultiDisplayMode();
        }
    }

    /**
     * @return The number of Chrome instances either running alive or dormant but the state is
     *     present for restoration.
     */
    @VisibleForTesting
    int getInstanceCount() {
        return mMultiWindowModeStateDispatcher.getInstanceCount();
    }

    /**
     * @return Whether the update Chrome menu item should be displayed.
     */
    protected boolean shouldShowUpdateMenuItem() {
        return UpdateMenuItemHelper.getInstance(mTabModelSelector.getModel(false).getProfile())
                        .getUiState()
                        .itemState
                != null;
    }

    /**
     * @return Whether the "Move to other window" menu item should be displayed.
     */
    protected boolean shouldShowMoveToOtherWindow() {
        if (!instanceSwitcherWithMultiInstanceEnabled() && shouldShowNewWindow()) return false;
        return mMultiWindowModeStateDispatcher.isMoveToOtherWindowSupported(mTabModelSelector);
    }

    private boolean shouldShowWebFeedMenuItem() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isIncognito() || OfflinePageUtils.isOfflinePage(tab)) {
            return false;
        }
        if (!FeedFeatures.isWebFeedUIEnabled(tab.getProfile())) {
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
        }
        return 0;
    }

    @Override
    public void onFooterViewInflated(AppMenuHandler appMenuHandler, View view) {
        if (view instanceof WebFeedMainMenuItem) {
            ((WebFeedMainMenuItem) view)
                    .initialize(
                            mActivityTabProvider.get(),
                            appMenuHandler,
                            WebFeedFaviconFetcher.createDefault(),
                            mFeedLauncher,
                            mModalDialogManager,
                            mSnackbarManager,
                            CreatorActivity.class);
        }
    }

    @Override
    public int getHeaderResourceId() {
        return 0;
    }

    @Override
    public void onHeaderViewInflated(AppMenuHandler appMenuHandler, View view) {}

    @Override
    public boolean shouldShowFooter(int maxMenuHeight) {
        if (shouldShowWebFeedMenuItem()) {
            return true;
        }
        return super.shouldShowFooter(maxMenuHeight);
    }

    @Override
    protected boolean shouldShowManagedByMenuItem(Tab currentTab) {
        return ManagedBrowserUtils.isBrowserManaged(currentTab.getProfile());
    }

    @Override
    public boolean shouldShowIconBeforeItem() {
        return true;
    }

    @Override
    public void onMenuDismissed() {
        super.onMenuDismissed();
        if (mUpdateMenuItemVisible) {
            UpdateMenuItemHelper updateHelper =
                    UpdateMenuItemHelper.getInstance(
                            mTabModelSelector.getModel(false).getProfile());
            updateHelper.onMenuDismissed();
            updateHelper.unregisterObserver(mAppMenuInvalidator);
            mUpdateMenuItemVisible = false;
            mAppMenuInvalidator = null;
        }
    }
}
