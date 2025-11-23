// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
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
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.pdf.PdfPage;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.supervised_user.SupervisedUserServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tinker_tank.TinkerTankDelegate;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.accessibility.AccessibilityFeatureMap;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.accessibility.PageZoomMenuItemCoordinator;
import org.chromium.components.browser_ui.accessibility.PageZoomProperties;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/** An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity. */
@NullMarked
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    @IntDef({
        TabbedAppMenuItemType.UPDATE_ITEM,
        TabbedAppMenuItemType.NEW_INCOGNITO,
        TabbedAppMenuItemType.ZOOM_ITEM
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabbedAppMenuItemType {
        /** Regular Android menu item that contains a title and an icon if icon is specified. */
        int UPDATE_ITEM = AppMenuHandler.AppMenuItemType.NUM_ENTRIES;

        /**
         * Menu item that has two buttons, the first one is a title and the second one is an icon.
         * It is different from the regular menu item because it contains two separate buttons.
         */
        int NEW_INCOGNITO = AppMenuHandler.AppMenuItemType.NUM_ENTRIES + 1;

        /** Menu item that has a title and two buttons. */
        int ZOOM_ITEM = AppMenuHandler.AppMenuItemType.NUM_ENTRIES + 2;
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

    private @Nullable Runnable mUpdateStateChangeObserver;

    private final CallbackController mIncognitoReauthCallbackController = new CallbackController();

    private final PageZoomMenuItemCoordinator mPageZoomMenuItemCoordinator;

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
            OneshotSupplier<IncognitoReauthController> incognitoReauthControllerOneshotSupplier,
            Supplier<ReadAloudController> readAloudControllerSupplier,
            PageZoomManager pageZoomManager) {
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
        mPageZoomMenuItemCoordinator = new PageZoomMenuItemCoordinator(pageZoomManager);

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
                TabbedAppMenuItemType.NEW_INCOGNITO,
                new LayoutViewBuilder(R.layout.custom_view_menu_item),
                IncognitoMenuItemViewBinder::bind);

        modelListAdapter.registerType(
                TabbedAppMenuItemType.ZOOM_ITEM,
                new LayoutViewBuilder(R.layout.page_zoom_menu_item),
                PageZoomMenuItemViewBinder::bind);
    }

    @Override
    public MVCListAdapter.ModelList buildMenuModelList() {
        int menuGroup = getMenuGroup();
        MVCListAdapter.ModelList modelList = new MVCListAdapter.ModelList();
        if (menuGroup == MenuGroup.PAGE_MENU) {
            populatePageModeMenu(modelList);
        } else if (menuGroup == MenuGroup.OVERVIEW_MODE_MENU) {
            populateOverviewModeMenu(modelList);
        } else if (menuGroup == MenuGroup.TABLET_EMPTY_MODE_MENU) {
            populateTabletEmptyModeMenu(modelList);
        }
        return modelList;
    }

    private void populatePageModeMenu(MVCListAdapter.ModelList modelList) {
        Tab currentTab = mActivityTabProvider.get();

        GURL url = currentTab != null ? currentTab.getUrl() : GURL.emptyGURL();
        final boolean isNativePage =
                url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                        || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)
                        || (currentTab != null && currentTab.isNativePage());
        final boolean isFileScheme = url.getScheme().equals(UrlConstants.FILE_SCHEME);
        final boolean isContentScheme = url.getScheme().equals(UrlConstants.CONTENT_SCHEME);

        if (shouldShowIconRow()) {
            List<PropertyModel> iconModels = new ArrayList<>();
            iconModels.add(buildForwardActionModel(currentTab));
            iconModels.add(buildBookmarkActionModel(currentTab));
            iconModels.add(buildDownloadActionModel(currentTab));
            iconModels.add(buildPageInfoModel(currentTab));
            iconModels.add(buildReloadModel(currentTab));

            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.BUTTON_ROW,
                            buildModelForIconRow(R.id.icon_row_menu_id, iconModels)));
        }

        mUpdateMenuItemVisible = shouldShowUpdateMenuItem();
        if (mUpdateMenuItemVisible) {
            modelList.add(buildUpdateItem());
            mUpdateStateChangeObserver = buildUpdateStateChangedObserver();
            UpdateMenuItemHelper.getInstance(getProfileFromTabModel())
                    .registerObserver(mUpdateStateChangeObserver);
        }

        // When the feature is enabled, show either "New Incognito tab" in incognito mode
        // or "New tab" in normal mode. When the feature is disabled, show both.
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
            modelList.add(buildNewTabItem());
        }
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || isIncognitoShowing()) {
            modelList.add(buildNewIncognitoTabItem());
        }

        // Add to Group
        if (shouldShowAddToGroup()) modelList.add(buildAddToGroupItem(currentTab));

        // New Window
        if (shouldShowNewWindow()) modelList.add(buildNewWindowItem());

        // New Incognito Window
        if (shouldShowNewIncognitoWindow()) modelList.add(buildNewIncognitoWindowItem());

        // Move to other window
        if (shouldShowMoveToOtherWindow()) modelList.add(buildMoveToOtherWindowItem());

        // Manage windows
        if (MultiWindowUtils.shouldShowManageWindowsMenu()) modelList.add(buildManageWindowsItem());

        // Divider
        maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Open History
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
            modelList.add(buildHistoryItem());
        }

        // Tinker Tank
        if (shouldShowTinkerTank()) modelList.add(buildTinkerTankItem());

        // Quick Delete
        if (shouldShowQuickDeleteItem()) {
            modelList.add(buildQuickDeleteItem());
            maybeAddDividerLine(modelList, R.id.quick_delete_divider_line_id);
        }

        // Downloads
        modelList.add(buildDownloadsItem());

        // Bookmarks
        modelList.add(buildBookmarksItem());

        // Recent Tabs
        if (shouldShowRecentTabsItem()) modelList.add(buildRecentTabsItem());

        // Extensions
        if (shouldShowExtensionsItem()) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
                modelList.add(buildExtensionsParentItem());
            } else {
                modelList.add(buildExtensionsItem());
            }
        }

        // Divider
        modelList.add(
                new MVCListAdapter.ListItem(
                        AppMenuHandler.AppMenuItemType.DIVIDER,
                        buildModelForDivider(R.id.divider_line_id)));

        // Page Zoom
        // Disable page zoom menu item on Reading Mode pages.
        if (shouldShowPageZoomItem(currentTab) && !isReaderModeShowing(currentTab)) {
            modelList.add(buildPageZoomItem(currentTab));
            // Divider
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.DIVIDER,
                            buildModelForDivider(R.id.divider_line_id)));
        }

        // Share
        if (ShareUtils.shouldEnableShare(currentTab)) {
            modelList.add(buildShareListItem(shouldShowIconBeforeItem()));
        }

        // Download Page
        if (shouldShowDownloadPageMenuItem(currentTab)) {
            modelList.add(buildDownloadPageItem(currentTab));
        }

        // Print
        if (shouldShowPrintItem(currentTab)) modelList.add(buildPrintItem(currentTab));

        // Price Tracking (enable / disable)
        MVCListAdapter.ListItem priceTrackingItem =
                maybeBuildPriceTrackingListItem(currentTab, shouldShowIconBeforeItem());
        if (priceTrackingItem != null) modelList.add(priceTrackingItem);

        // AI / AI PDF
        MVCListAdapter.ListItem aiItem = maybeBuildAiMenuItem(currentTab);
        if (aiItem != null) modelList.add(aiItem);

        // Find in page
        if (shouldShowFindInPageItem(currentTab)) modelList.add(buildFindInPageItem(currentTab));

        // Translate
        if (shouldShowTranslateMenuItem(currentTab)) {
            modelList.add(buildTranslateMenuItem(currentTab, shouldShowIconBeforeItem()));
        }

        // Readaloud
        observeAndMaybeAddReadAloud(modelList, currentTab);

        // Reader mode
        if (shouldShowReaderModeItem(currentTab)) {
            modelList.add(buildReaderModeItem(currentTab));
        }

        // Open with ...
        if (shouldShowOpenWithItem(currentTab)) {
            modelList.add(buildOpenWithItem(currentTab, shouldShowIconBeforeItem()));
        }

        // Universal Install / Open Web APK
        if (shouldShowHomeScreenMenuItem(
                isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)) {
            modelList.add(buildAddToHomescreenListItem(currentTab, shouldShowIconBeforeItem()));
        }

        // RDS
        MVCListAdapter.ListItem rdsListItem =
                maybeBuildRequestDesktopSiteListItem(
                        currentTab, isNativePage, shouldShowIconBeforeItem());
        if (rdsListItem != null) modelList.add(rdsListItem);

        // Auto Dark
        if (shouldShowAutoDarkItem(currentTab, isNativePage)) {
            modelList.add(buildAutoDarkItem(currentTab, isNativePage, shouldShowIconBeforeItem()));
        }

        // Paint Preview
        if (shouldShowPaintPreview(isNativePage, currentTab)) {
            modelList.add(buildPaintPreviewItem(isNativePage, currentTab));
        }

        // Get Image Descriptions
        if (shouldShowGetImageDescriptionsItem(currentTab)) {
            modelList.add(buildGetImageDescriptionsItem(currentTab));
        }

        // Listen to the Feed
        if (shouldShowListenToFeedItem(currentTab)) {
            modelList.add(buildListenToFeedItem());
        }

        // Divider Line
        maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Reader Mode Prefs
        if (shouldShowReaderModePrefs(currentTab)) {
            modelList.add(buildReaderModePrefsItem());
        }

        // Settings
        modelList.add(buildSettingsItem());

        // NTP Customizations
        if (shouldShowNtpCustomizations(currentTab)) {
            modelList.add(buildNtpCustomizationsItem(currentTab));
        }

        // Help
        modelList.add(buildHelpItem());

        // Managed by
        if (shouldShowManagedByMenuItem(currentTab)) {
            maybeAddDividerLine(modelList, R.id.managed_by_divider_line_id);
            modelList.add(buildManagedByItem(currentTab));
        }
        if (shouldShowContentFilterHelpCenterMenuItem(currentTab)) {
            maybeAddDividerLine(modelList, R.id.menu_item_content_filter_divider_line_id);
            modelList.add(buildContentFilterHelpCenterMenuItem(currentTab));
        }
    }

    private Runnable buildUpdateStateChangedObserver() {
        return () -> {
            MVCListAdapter.ModelList modelList = getModelList();
            if (modelList == null) {
                assert false : "ModelList should not be null";
                return;
            }
            for (MVCListAdapter.ListItem listItem : modelList) {
                if (listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == R.id.update_menu_id) {
                    updateUpdateItemData(listItem.model);
                    return;
                }
            }
        };
    }

    private void maybeAddDividerLine(MVCListAdapter.ModelList modelList, @IdRes int id) {
        if (modelList.get(modelList.size() - 1).type == AppMenuHandler.AppMenuItemType.DIVIDER) {
            return;
        }

        modelList.add(
                new MVCListAdapter.ListItem(
                        AppMenuHandler.AppMenuItemType.DIVIDER, buildModelForDivider(id)));
    }

    private void populateOverviewModeMenu(MVCListAdapter.ModelList modelList) {
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
            modelList.add(buildNewTabItem());
        }
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || isIncognitoShowing()) {
            modelList.add(buildNewIncognitoTabItem());
        }
        if (shouldShowNewIncognitoWindow()) {
            modelList.add(buildNewWindowItem());
            modelList.add(buildNewIncognitoWindowItem());
        }
        if (ChromeFeatureList.sTabGroupEntryPointsAndroid.isEnabled()) {
            modelList.add(buildNewTabGroupItem());
        }
        modelList.add(buildCloseAllTabsItem());
        if (shouldShowTinkerTank()) modelList.add(buildTinkerTankItem());
        modelList.add(buildSelectTabsItem());
        if (shouldShowQuickDeleteItem()) modelList.add(buildQuickDeleteItem());
        modelList.add(buildSettingsItem());
    }

    private void populateTabletEmptyModeMenu(MVCListAdapter.ModelList modelList) {
        modelList.add(buildNewTabItem());
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            modelList.add(buildNewWindowItem());
            modelList.add(buildNewIncognitoWindowItem());
        } else {
            modelList.add(buildNewIncognitoTabItem());
        }
        modelList.add(buildSettingsItem());
        if (shouldShowQuickDeleteItem()) modelList.add(buildQuickDeleteItem());
    }

    private MVCListAdapter.ListItem buildUpdateItem() {
        assert shouldShowUpdateMenuItem();
        PropertyModel model =
                populateBaseModelForTextItem(
                                new PropertyModel.Builder(UpdateMenuItemViewBinder.ALL_KEYS),
                                R.id.update_menu_id)
                        .with(AppMenuItemProperties.TITLE, mContext.getString(R.string.menu_update))
                        .with(
                                AppMenuItemProperties.ICON,
                                AppCompatResources.getDrawable(mContext, R.drawable.menu_update))
                        .build();
        updateUpdateItemData(model);
        return new MVCListAdapter.ListItem(TabbedAppMenuItemType.UPDATE_ITEM, model);
    }

    private void updateUpdateItemData(PropertyModel model) {
        MenuItemState itemState =
                UpdateMenuItemHelper.getInstance(getProfileFromTabModel()).getUiState().itemState;
        if (itemState == null) {
            assert false : "The update state should be non-null";
            model.set(AppMenuItemProperties.ENABLED, false);
            return;
        }
        model.set(UpdateMenuItemViewBinder.SUMMARY, itemState.summary);
        model.set(AppMenuItemProperties.TITLE, mContext.getString(itemState.title));
        model.set(UpdateMenuItemViewBinder.TITLE_COLOR_ID, itemState.titleColorId);
        Drawable icon = null;
        if (itemState.icon != 0) {
            icon = AppCompatResources.getDrawable(mContext, itemState.icon);
        }
        if (icon != null && itemState.iconTintId != 0) {
            DrawableCompat.setTint(icon, mContext.getColor(itemState.iconTintId));
        }
        model.set(AppMenuItemProperties.ICON, icon);
        model.set(AppMenuItemProperties.ENABLED, itemState.enabled);
    }

    private MVCListAdapter.ListItem buildNewTabItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.new_tab_menu_id,
                        R.string.menu_new_tab,
                        shouldShowIconBeforeItem() ? R.drawable.ic_add_box_rounded_corner : 0));
    }

    private boolean isIncognitoShowing() {
        return mTabModelSelector.getCurrentModel().isIncognito();
    }

    private boolean isIncognitoReauthShowing() {
        return isIncognitoShowing()
                && (mIncognitoReauthController != null)
                && mIncognitoReauthController.isReauthPageShowing();
    }

    private MVCListAdapter.ListItem buildNewIncognitoTabItem() {
        int iconRes = 0;
        if (shouldShowIconBeforeItem()) {
            iconRes =
                    IncognitoUtils.shouldOpenIncognitoAsWindow()
                            ? R.drawable.ic_add_box_rounded_corner
                            : R.drawable.ic_incognito;
        }
        PropertyModel model =
                buildModelForStandardMenuItem(
                        R.id.new_incognito_tab_menu_id, R.string.menu_new_incognito_tab, iconRes);
        model.set(
                AppMenuItemProperties.ENABLED, isIncognitoEnabled() && !isIncognitoReauthShowing());
        return new MVCListAdapter.ListItem(TabbedAppMenuItemType.NEW_INCOGNITO, model);
    }

    private boolean shouldShowAddToGroup() {
        return ChromeFeatureList.sTabGroupParityBottomSheetAndroid.isEnabled()
                && (!ChromeFeatureList.sTabModelInitFixes.isEnabled()
                        || mTabModelSelector.isTabStateInitialized());
    }

    private MVCListAdapter.ListItem buildAddToGroupItem(Tab currentTab) {
        assert shouldShowAddToGroup();
        PropertyModel model =
                buildModelForStandardMenuItem(
                        R.id.add_to_group_menu_id,
                        R.string.menu_add_tab_to_group,
                        shouldShowIconBeforeItem() ? R.drawable.ic_widgets : 0);
        model.set(
                AppMenuItemProperties.TITLE,
                mContext.getString(
                        getAddToGroupMenuItemString(
                                currentTab != null ? currentTab.getTabGroupId() : null)));
        return new MVCListAdapter.ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private MVCListAdapter.ListItem buildNewWindowItem() {
        assert shouldShowNewWindow();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.new_window_menu_id,
                        R.string.menu_new_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_new_window : 0));
    }

    private MVCListAdapter.ListItem buildNewIncognitoWindowItem() {
        assert shouldShowNewIncognitoWindow();
        PropertyModel model =
                buildModelForStandardMenuItem(
                        R.id.new_incognito_window_menu_id,
                        R.string.menu_new_incognito_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_incognito : 0);
        model.set(
                AppMenuItemProperties.ENABLED, isIncognitoEnabled() && !isIncognitoReauthShowing());
        return new MVCListAdapter.ListItem(TabbedAppMenuItemType.NEW_INCOGNITO, model);
    }

    private MVCListAdapter.ListItem buildMoveToOtherWindowItem() {
        assert shouldShowMoveToOtherWindow();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.move_to_other_window_menu_id,
                        R.string.menu_move_to_other_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_open_in_browser : 0));
    }

    private MVCListAdapter.ListItem buildManageWindowsItem() {
        assert MultiWindowUtils.shouldShowManageWindowsMenu();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.manage_all_windows_menu_id,
                        R.string.menu_manage_all_windows,
                        shouldShowIconBeforeItem() ? R.drawable.ic_select_window : 0));
    }

    private MVCListAdapter.ListItem buildHistoryItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.open_history_menu_id,
                        R.string.menu_history,
                        shouldShowIconBeforeItem() ? R.drawable.ic_history_googblue_24dp : 0));
    }

    private MVCListAdapter.ListItem buildDownloadsItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.downloads_menu_id,
                        R.string.menu_downloads,
                        shouldShowIconBeforeItem() ? R.drawable.infobar_download_complete : 0));
    }

    private MVCListAdapter.ListItem buildBookmarksItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.all_bookmarks_menu_id,
                        R.string.menu_bookmarks,
                        shouldShowIconBeforeItem() ? R.drawable.btn_star_filled : 0));
    }

    private boolean shouldShowRecentTabsItem() {
        return !isIncognitoShowing();
    }

    private MVCListAdapter.ListItem buildRecentTabsItem() {
        assert shouldShowRecentTabsItem();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.recent_tabs_menu_id,
                        R.string.menu_recent_tabs,
                        shouldShowIconBeforeItem() ? R.drawable.devices_black_24dp : 0));
    }

    private boolean shouldShowExtensionsItem() {
        // TODO(crbug.com/422307625): Remove this check once extensions are ready for dogfooding.
        return ExtensionUi.isEnabled(getProfileFromTabModel());
    }

    private MVCListAdapter.ListItem buildExtensionsItem() {
        assert shouldShowExtensionsItem();

        // The id {@code R.id.extensions_menu_id} is used for both when this flag is enabled and
        // disabled but in different context.
        assert !ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);

        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.extensions_menu_id,
                        R.string.menu_extensions,
                        shouldShowIconBeforeItem() ? R.drawable.ic_extension_24dp : 0));
    }

    private MVCListAdapter.ListItem buildExtensionsParentItem() {
        assert shouldShowExtensionsItem();

        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildManageExtensionsItem());
        submenuItems.add(buildChromeWebstoreItem());

        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.extensions_parent_menu_id,
                        R.string.menu_extensions,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_extension_24dp
                                : Resources.ID_NULL,
                        submenuItems));
    }

    private MVCListAdapter.ListItem buildManageExtensionsItem() {
        assert shouldShowExtensionsItem();

        // The id {@code R.id.extensions_menu_id} is used for both when this flag is enabled and
        // disabled but in different context.
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);

        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.extensions_menu_id,
                        R.string.menu_manage_extensions,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_extension_24dp
                                : Resources.ID_NULL));
    }

    private MVCListAdapter.ListItem buildChromeWebstoreItem() {
        assert shouldShowExtensionsItem();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.extensions_webstore_menu_id,
                        R.string.menu_chrome_webstore,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_webstore_menu
                                : Resources.ID_NULL));
    }

    private boolean shouldShowPageZoomItem(Tab currentTab) {
        return currentTab != null
                && shouldShowWebContentsDependentMenuItem(currentTab)
                && PageZoomUtils.shouldShowZoomMenuItem();
    }

    private boolean shouldShowLFFPageZoomItem() {
        return AccessibilityFeatureMap.sAndroidZoomIndicator.isEnabled()
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    private PropertyModel buildNewPageZoomModel() {
        PropertyKey[] keys =
                PropertyModel.concatKeys(
                        AppMenuItemProperties.ALL_KEYS, PageZoomProperties.ALL_KEYS_FOR_MENU_ITEM);
        Drawable icon =
                shouldShowIconBeforeItem()
                        ? AppCompatResources.getDrawable(mContext, R.drawable.ic_zoom)
                        : null;
        PropertyModel model =
                populateBaseModelForTextItem(new PropertyModel.Builder(keys), R.id.page_zoom_id)
                        .with(
                                AppMenuItemProperties.TITLE,
                                mContext.getString(R.string.page_zoom_menu_title))
                        .with(AppMenuItemProperties.MENU_ITEM_ID, R.id.page_zoom_id)
                        .with(AppMenuItemProperties.ICON, icon)
                        .build();
        return model;
    }

    private MVCListAdapter.ListItem buildPageZoomItem(Tab currentTab) {
        assert shouldShowPageZoomItem(currentTab);
        if (shouldShowLFFPageZoomItem()) {
            PropertyModel model = buildNewPageZoomModel();
            mPageZoomMenuItemCoordinator.setModel(model);
            return new MVCListAdapter.ListItem(TabbedAppMenuItemType.ZOOM_ITEM, model);
        }
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.page_zoom_id,
                        R.string.page_zoom_menu_title,
                        shouldShowIconBeforeItem() ? R.drawable.ic_zoom : 0));
    }

    private MVCListAdapter.ListItem buildDownloadPageItem(Tab currentTab) {
        assert shouldShowDownloadPageMenuItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.download_page_id,
                        R.string.menu_download_page,
                        shouldShowIconBeforeItem() ? R.drawable.ic_file_download_white_24dp : 0));
    }

    /** Determines whether the "Print" menu item should be shown for a given tab. */
    private boolean shouldShowPrintItem(@Nullable Tab currentTab) {
        // A tab must exist to print from it.
        if (currentTab == null) {
            return false;
        }

        // Check if sharing (which includes printing) is generally enabled for this tab's content.
        boolean canShareTab = ShareUtils.shouldEnableShare(currentTab);

        // Check if printing is specifically enabled in user preferences for the current profile.
        Profile profile = currentTab.getProfile();
        boolean isPrintingEnabled = UserPrefs.get(profile).getBoolean(Pref.PRINTING_ENABLED);

        // Show print item if we're on a desktop device or if the current tab is displaying a native
        // PDF.
        NativePage nativePage = currentTab.getNativePage();
        boolean isPdfPage = nativePage != null && nativePage.isPdf();
        boolean isEligibleForPrint = DeviceInfo.isDesktop() || isPdfPage;

        return canShareTab && isPrintingEnabled && isEligibleForPrint;
    }

    private MVCListAdapter.ListItem buildPrintItem(Tab currentTab) {
        assert shouldShowPrintItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.print_id,
                        R.string.menu_print,
                        shouldShowIconBeforeItem() ? R.drawable.sharing_print : 0));
    }

    private boolean shouldShowReaderModeItem(@Nullable Tab currentTab) {
        if (currentTab == null) {
            return false;
        }

        GURL url = currentTab.getUrl();
        boolean isChromeOrNativePage =
                url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                        || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)
                        || currentTab.isNativePage();

        if (isChromeOrNativePage) {
            return false;
        }

        return (DomDistillerFeatures.showAlwaysOnEntryPoint()
                || DomDistillerFeatures.sReaderModeDistillInApp.isEnabled());
    }

    private boolean shouldShowGetImageDescriptionsItem(@Nullable Tab currentTab) {
        return currentTab != null
                && shouldShowWebContentsDependentMenuItem(currentTab)
                && ImageDescriptionsController.getInstance().shouldShowImageDescriptionsMenuItem();
    }

    private MVCListAdapter.ListItem buildGetImageDescriptionsItem(Tab currentTab) {
        assert shouldShowGetImageDescriptionsItem(currentTab);

        @StringRes int titleId = R.string.menu_stop_image_descriptions;
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

        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.get_image_descriptions_id,
                        titleId,
                        shouldShowIconBeforeItem() ? R.drawable.ic_image_descriptions : 0));
    }

    private MVCListAdapter.ListItem buildNewTabGroupItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.new_tab_group_menu_id,
                        R.string.menu_new_tab_group,
                        shouldShowIconBeforeItem() ? R.drawable.ic_widgets : 0));
    }

    private MVCListAdapter.ListItem buildCloseAllTabsItem() {
        final PropertyModel model;
        if (isIncognitoShowing()) {
            model =
                    buildModelForStandardMenuItem(
                            R.id.close_all_incognito_tabs_menu_id,
                            R.string.menu_close_all_incognito_tabs,
                            shouldShowIconBeforeItem() ? R.drawable.ic_close_all_tabs : 0);
            model.set(
                    AppMenuItemProperties.ENABLED, mTabModelSelector.getModel(true).getCount() > 0);
        } else {
            model =
                    buildModelForStandardMenuItem(
                            R.id.close_all_tabs_menu_id,
                            R.string.menu_close_all_tabs,
                            shouldShowIconBeforeItem() ? R.drawable.btn_close_white : 0);
            model.set(AppMenuItemProperties.ENABLED, mTabModelSelector.getTotalTabCount() > 0);
        }
        return new MVCListAdapter.ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private boolean shouldShowTinkerTank() {
        return TinkerTankDelegate.isEnabled();
    }

    private MVCListAdapter.ListItem buildTinkerTankItem() {
        assert shouldShowTinkerTank();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.tinker_tank_menu_id,
                        R.string.menu_tinker_tank,
                        shouldShowIconBeforeItem() ? R.drawable.ic_add_box_rounded_corner : 0));
    }

    private MVCListAdapter.ListItem buildSelectTabsItem() {
        PropertyModel model =
                buildModelForStandardMenuItem(
                        R.id.menu_select_tabs,
                        R.string.menu_select_tabs,
                        shouldShowIconBeforeItem() ? R.drawable.ic_select_check_box_24dp : 0);
        boolean isEnabled =
                !isIncognitoReauthShowing()
                        && mTabModelSelector.isTabStateInitialized()
                        && mTabModelSelector.getCurrentModel().getCount() != 0;
        model.set(AppMenuItemProperties.ENABLED, isEnabled);

        return new MVCListAdapter.ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private MVCListAdapter.ListItem buildSettingsItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.preferences_id,
                        R.string.menu_settings,
                        shouldShowIconBeforeItem() ? R.drawable.settings_cog : 0));
    }

    private boolean shouldShowListenToFeedItem(@Nullable Tab currentTab) {
        if (currentTab == null
                || isIncognitoShowing()
                || !UrlUtilities.isNtpUrl(currentTab.getUrl())
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_AUDIO_OVERVIEWS)) {
            return false;
        }

        Profile profile = currentTab.getProfile();
        if (!FeedFeatures.isFeedEnabled(profile)
                || !UserPrefs.get(profile).getBoolean(Pref.ARTICLES_LIST_VISIBLE)) {
            return false;
        }

        ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
        return readAloudController != null && readAloudController.isAvailable();
    }

    private MVCListAdapter.ListItem buildListenToFeedItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.listen_to_feed_id,
                        R.string.menu_listen_to_feed,
                        R.drawable.ic_play_circle));
    }

    /**
     * Returns True if the NTP Customization menu entry should be visible.
     *
     * <p>This entry is shown only when the corresponding feature flag is enabled and the user is on
     * the regular Ntp.
     */
    private boolean shouldShowNtpCustomizations(@Nullable Tab currentTab) {
        return ChromeFeatureList.sNewTabPageCustomization.isEnabled()
                && !isIncognitoShowing()
                && currentTab != null
                && UrlUtilities.isNtpUrl(currentTab.getUrl());
    }

    private MVCListAdapter.ListItem buildNtpCustomizationsItem(Tab currentTab) {
        assert shouldShowNtpCustomizations(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.ntp_customization_id,
                        R.string.menu_ntp_customization,
                        shouldShowIconBeforeItem() ? R.drawable.bookmark_edit_active : 0));
    }

    private MVCListAdapter.ListItem buildHelpItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.help_id,
                        R.string.menu_help,
                        shouldShowIconBeforeItem() ? R.drawable.help_outline : 0));
    }

    private boolean shouldShowQuickDeleteItem() {
        return !isIncognitoShowing();
    }

    private MVCListAdapter.ListItem buildQuickDeleteItem() {
        assert shouldShowQuickDeleteItem();
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.quick_delete_menu_id,
                        R.string.menu_quick_delete,
                        shouldShowIconBeforeItem() ? R.drawable.material_ic_delete_24dp : 0));
    }

    @VisibleForTesting
    boolean shouldShowIconRow() {
        boolean shouldShowIconRow = true;
        if (mIsTablet) {
            boolean widthOnTabletBelowMinimum =
                    mDecorView.getWidth()
                            < DeviceFormFactor.getNonMultiDisplayMinimumTabletWidthPx(mContext);
            boolean appMenuIconsHiddenForWidth =
                    ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled()
                            && mToolbarManager.areAnyToolbarComponentsMissingForWidth(
                                    ToolbarUtils.APP_MENU_ICON_ROW_COMPONENTS);
            shouldShowIconRow = widthOnTabletBelowMinimum || appMenuIconsHiddenForWidth;
        }

        final boolean isMenuButtonOnTop = mToolbarManager != null;
        shouldShowIconRow &= isMenuButtonOnTop;
        return shouldShowIconRow;
    }

    private boolean shouldShowFindInPageItem(@Nullable Tab currentTab) {
        return currentTab != null
                && (shouldShowWebContentsDependentMenuItem(currentTab)
                        || (currentTab.isNativePage()
                                && assumeNonNull(currentTab.getNativePage()).isPdf()));
    }

    private MVCListAdapter.ListItem buildFindInPageItem(@Nullable Tab currentTab) {
        assert shouldShowFindInPageItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.find_in_page_id,
                        R.string.menu_find_in_page,
                        shouldShowIconBeforeItem() ? R.drawable.ic_find_in_page : 0));
    }

    private MVCListAdapter.@Nullable ListItem maybeBuildAiMenuItem(@Nullable Tab currentTab) {
        if (currentTab == null
                || currentTab.getWebContents() == null
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY)
                || !AiAssistantService.getInstance().canShowAiForTab(mContext, currentTab)) {
            return null;
        }

        if (currentTab.isNativePage() && currentTab.getNativePage() instanceof PdfPage) {
            return new MVCListAdapter.ListItem(
                    AppMenuHandler.AppMenuItemType.STANDARD,
                    buildModelForStandardMenuItem(
                            R.id.ai_pdf_menu_id,
                            R.string.menu_review_pdf_with_ai,
                            shouldShowIconBeforeItem() ? R.drawable.summarize_auto : 0));
        } else if (currentTab.getUrl() != null && UrlUtilities.isHttpOrHttps(currentTab.getUrl())) {
            return new MVCListAdapter.ListItem(
                    AppMenuHandler.AppMenuItemType.STANDARD,
                    buildModelForStandardMenuItem(
                            R.id.ai_web_menu_id,
                            R.string.menu_summarize_with_ai,
                            shouldShowIconBeforeItem() ? R.drawable.summarize_auto : 0));
        }
        return null;
    }

    /**
     * @param isNativePage Whether the current tab is a native page.
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the paint preview menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowPaintPreview(boolean isNativePage, @Nullable Tab currentTab) {
        return currentTab != null
                && ChromeFeatureList.sPaintPreviewDemo.isEnabled()
                && !isNativePage
                && !isIncognitoShowing();
    }

    private MVCListAdapter.ListItem buildPaintPreviewItem(boolean isNativePage, Tab currentTab) {
        assert shouldShowPaintPreview(isNativePage, currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.paint_preview_show_id,
                        R.string.menu_paint_preview_show,
                        shouldShowIconBeforeItem() ? R.drawable.ic_photo_camera : 0));
    }

    /**
     * @return Whether the "New window" menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowNewWindow() {
        // Hide the menu on automotive devices.
        if (DeviceInfo.isAutomotive()) return false;

        if (instanceSwitcherWithMultiInstanceEnabled()) {
            // Hide the menu if we already have the maximum number of windows.
            if (MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ACTIVE)
                    >= MultiWindowUtils.getMaxInstances()) return false;

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
     * @return Whether the "New incognito window" menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowNewIncognitoWindow() {
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            return false;
        }

        return shouldShowNewWindow();
    }

    /**
     * @return Whether the update Chrome menu item should be displayed.
     */
    protected boolean shouldShowUpdateMenuItem() {
        return UpdateMenuItemHelper.getInstance(getProfileFromTabModel()).getUiState().itemState
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
    public @Nullable View buildFooterView(AppMenuHandler appMenuHandler) {
        if (!shouldShowWebFeedMenuItem()) {
            return null;
        }

        WebFeedMainMenuItem footer =
                (WebFeedMainMenuItem)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.web_feed_main_menu_item, null);
        footer.initialize(
                mActivityTabProvider.get(),
                appMenuHandler,
                WebFeedFaviconFetcher.createDefault(),
                mFeedLauncher,
                mModalDialogManager,
                mSnackbarManager,
                CreatorActivity.class);
        return footer;
    }

    @VisibleForTesting
    protected boolean shouldShowManagedByMenuItem(@Nullable Tab currentTab) {
        return currentTab != null && ManagedBrowserUtils.isBrowserManaged(currentTab.getProfile());
    }

    protected boolean shouldShowContentFilterHelpCenterMenuItem(@Nullable Tab currentTab) {
        return currentTab != null
                && SupervisedUserServiceBridge.isSupervisedLocally(currentTab.getProfile());
    }

    private MVCListAdapter.ListItem buildManagedByItem(Tab currentTab) {
        assert shouldShowManagedByMenuItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.managed_by_menu_id,
                        R.string.managed_browser,
                        shouldShowIconBeforeItem() ? R.drawable.ic_business : 0));
    }

    private MVCListAdapter.ListItem buildContentFilterHelpCenterMenuItem(Tab currentTab) {
        assert shouldShowContentFilterHelpCenterMenuItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.menu_item_content_filter_help_center_id,
                        R.string.menu_item_content_filter_help_center_link,
                        shouldShowIconBeforeItem() ? R.drawable.ic_account_child_20dp : 0));
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
                    UpdateMenuItemHelper.getInstance(getProfileFromTabModel());
            updateHelper.onMenuDismissed();
            updateHelper.unregisterObserver(assumeNonNull(mUpdateStateChangeObserver));
            mUpdateMenuItemVisible = false;
            mUpdateStateChangeObserver = null;
        }
    }

    private Profile getProfileFromTabModel() {
        var profile = mTabModelSelector.getModel(false).getProfile();
        assert profile != null;
        return profile;
    }
}
