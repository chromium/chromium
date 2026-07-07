// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.SparseArray;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemUtils;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.open_in_app.OpenInAppMenuItemProvider;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.supervised_user.SupervisedUserServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.lens.LensOverlayTabHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.Supplier;

/** An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity. */
@NullMarked
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {

    @IntDef({TabbedAppMenuItemType.UPDATE_ITEM, TabbedAppMenuItemType.NEW_INCOGNITO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabbedAppMenuItemType {
        /** Regular Android menu item that contains a title and an icon if icon is specified. */
        int UPDATE_ITEM = AppMenuHandler.AppMenuItemType.NUM_ENTRIES + 1;

        /**
         * Menu item that has two buttons, the first one is a title and the second one is an icon.
         * It is different from the regular menu item because it contains two separate buttons.
         */
        int NEW_INCOGNITO = AppMenuHandler.AppMenuItemType.NUM_ENTRIES + 2;
    }

    AppMenuDelegate mAppMenuDelegate;
    ModalDialogManager mModalDialogManager;
    SnackbarManager mSnackbarManager;
    private final TabGroupItemBuilder mTabGroupItemBuilder;
    private final HistoryItemBuilder mHistoryItemBuilder;
    private SaveAndShareItemBuilder mSaveAndShareItemBuilder;
    private final MoreToolsItemBuilder mMoreToolsItemBuilder;

    private boolean mUpdateMenuItemVisible;

    /**
     * This is non null for the case of ChromeTabbedActivity when the corresponding {@link
     * CallbackController} has been fired.
     */
    private @Nullable IncognitoReauthController mIncognitoReauthController;

    private @Nullable Runnable mUpdateStateChangeObserver;

    private final CallbackController mIncognitoReauthCallbackController = new CallbackController();

    private final OneshotSupplier<HubManager> mHubManagerSupplier;

    private final BookmarksItemBuilder mBookmarksItemBuilder;
    private @Nullable FaviconHelper mFaviconHelper;
    private final FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
    private final RoundedIconGenerator mRoundedIconGenerator;
    private final Supplier<RecentlyClosedEntriesManager> mRecentlyClosedEntriesManagerSupplier;
    private final Supplier<SideUiStateProvider> mSideUiStateProviderSupplier;

    public TabbedAppMenuPropertiesDelegate(
            Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager,
            View decorView,
            AppMenuDelegate appMenuDelegate,
            OneshotSupplier<LayoutStateProvider> layoutStateProvider,
            NullableObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            ModalDialogManager modalDialogManager,
            SnackbarManager snackbarManager,
            OneshotSupplier<IncognitoReauthController> incognitoReauthControllerOneshotSupplier,
            MonotonicObservableSupplier<ReadAloudController> readAloudControllerSupplier,
            PageZoomManager pageZoomManager,
            OneshotSupplier<HubManager> hubManagerSupplier,
            @Nullable OpenInAppMenuItemProvider openInAppMenuItemProvider,
            Supplier<RecentlyClosedEntriesManager> recentlyClosedEntriesManagerSupplier,
            Supplier<SideUiStateProvider> sideUiStateProviderSupplier) {
        super(
                context,
                activityTabProvider,
                multiWindowModeStateDispatcher,
                tabModelSelector,
                toolbarManager,
                decorView,
                layoutStateProvider,
                bookmarkModelSupplier,
                readAloudControllerSupplier,
                pageZoomManager,
                openInAppMenuItemProvider);
        mAppMenuDelegate = appMenuDelegate;
        mModalDialogManager = modalDialogManager;
        mSnackbarManager = snackbarManager;
        mHubManagerSupplier = hubManagerSupplier;
        mDefaultFaviconHelper = new FaviconHelper.DefaultFaviconHelper();
        mRoundedIconGenerator = FaviconUtils.createCircularIconGenerator(mContext);
        mRecentlyClosedEntriesManagerSupplier = recentlyClosedEntriesManagerSupplier;
        mSideUiStateProviderSupplier = sideUiStateProviderSupplier;

        incognitoReauthControllerOneshotSupplier.onAvailable(
                mIncognitoReauthCallbackController.makeCancelable(
                        incognitoReauthController -> {
                            mIncognitoReauthController = incognitoReauthController;
                        }));

        mTabGroupItemBuilder =
                new TabGroupItemBuilder(
                        context,
                        getAppMenuItemTheme(),
                        tabModelSelector,
                        isMenuIconAtStart(),
                        shouldShowIconBeforeItem(),
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        this::getFaviconHelper);

        mHistoryItemBuilder =
                new HistoryItemBuilder(
                        context,
                        getAppMenuItemTheme(),
                        tabModelSelector,
                        this::getFaviconHelper,
                        mRecentlyClosedEntriesManagerSupplier,
                        isMenuIconAtStart(),
                        shouldShowIconBeforeItem(),
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper);

        mBookmarksItemBuilder =
                new BookmarksItemBuilder(
                        context,
                        getAppMenuItemTheme(),
                        mBookmarkModelSupplier,
                        tabModelSelector,
                        isMenuIconAtStart(),
                        shouldShowIconBeforeItem());

        mSaveAndShareItemBuilder =
                new SaveAndShareItemBuilder(
                        context, getAppMenuItemTheme(), isMenuIconAtStart(), tabModelSelector);

        mMoreToolsItemBuilder =
                new MoreToolsItemBuilder(
                        context,
                        getAppMenuItemTheme(),
                        isMenuIconAtStart(),
                        tabModelSelector,
                        readAloudControllerSupplier,
                        this::shouldShowPageInfoItem);
    }

    @Override
    public void registerCustomViewBinders(
            ModelListAdapter modelListAdapter,
            SparseArray<BiFunction<Context, PropertyModel, Integer>> customSizingSuppliers) {
        super.registerCustomViewBinders(modelListAdapter, customSizingSuppliers);
        modelListAdapter.registerType(
                TabbedAppMenuItemType.UPDATE_ITEM,
                new LayoutViewBuilder<>(R.layout.update_menu_item),
                UpdateMenuItemViewBinder::bind);
        customSizingSuppliers.append(
                TabbedAppMenuItemType.UPDATE_ITEM, UpdateMenuItemViewBinder::getPixelHeight);

        modelListAdapter.registerType(
                TabbedAppMenuItemType.NEW_INCOGNITO,
                new LayoutViewBuilder<>(R.layout.custom_view_menu_item),
                IncognitoMenuItemViewBinder::bind);
    }

    private FaviconHelper getFaviconHelper() {
        if (mFaviconHelper == null) {
            mFaviconHelper = new FaviconHelper();
        }
        return mFaviconHelper;
    }

    void setForeignSessionHelperForTesting(ForeignSessionHelper helper) {
        mHistoryItemBuilder.setForeignSessionHelperForTesting(helper);
    }

    @Override
    public void destroy() {
        super.destroy();
        mBookmarksItemBuilder.destroy();
        mHistoryItemBuilder.destroy();
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }
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
            if (ChromeFeatureList.sThreeDotMenuBackButton.isEnabled()) {
                iconModels.add(buildBackwardActionModel(currentTab));
            }
            iconModels.add(buildForwardActionModel(currentTab));
            iconModels.add(buildBookmarkActionModel(currentTab));
            iconModels.add(buildDownloadActionModel(currentTab));
            if (!ChromeFeatureList.sThreeDotMenuBackButton.isEnabled()) {
                iconModels.add(buildPageInfoModel(currentTab));
            }

            iconModels.add(buildReloadModel(currentTab));

            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.BUTTON_ROW,
                            AppMenuItemUtils.buildModelForIconRow(
                                    R.id.icon_row_menu_id, iconModels, isMenuIconAtStart())));
        }

        mUpdateMenuItemVisible = shouldShowUpdateMenuItem();
        if (mUpdateMenuItemVisible) {
            modelList.add(buildUpdateItem());
            mUpdateStateChangeObserver = buildUpdateStateChangedObserver();
            UpdateMenuItemHelper.getInstance(getProfileFromTabModel())
                    .registerObserver(mUpdateStateChangeObserver);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            populatePageModeMenuWithSubmenus(
                    modelList, currentTab, url, isNativePage, isFileScheme, isContentScheme);
        } else {
            populatePageModeMenuWithoutSubmenus(
                    modelList, currentTab, url, isNativePage, isFileScheme, isContentScheme);
        }
    }

    private void populatePageModeMenuWithoutSubmenus(
            MVCListAdapter.ModelList modelList,
            @Nullable Tab currentTab,
            GURL url,
            boolean isNativePage,
            boolean isFileScheme,
            boolean isContentScheme) {
        boolean separateIncognitoWindow = IncognitoUtils.shouldOpenIncognitoAsWindow();
        boolean isIncognito = isIncognitoShowing();
        if (!separateIncognitoWindow || !isIncognito) {
            modelList.add(buildNewTabItem());
        }

        if (!separateIncognitoWindow || isIncognito) {
            modelList.add(buildNewIncognitoTabItem());
        }

        // Add to Group
        boolean shouldShowIconBeforeItem = shouldShowIconBeforeItem();
        if (mTabGroupItemBuilder.shouldShowAddToGroup()) {
            modelList.add(
                    mTabGroupItemBuilder.buildAddToGroupItem(currentTab, shouldShowIconBeforeItem));
        }

        // New Window
        if (shouldShowNewWindow()) modelList.add(buildNewWindowItem());

        // New Incognito Window
        if (shouldShowNewIncognitoWindow()) modelList.add(buildNewIncognitoWindowItem());

        // Move to other window
        if (shouldShowMoveToOtherWindow()) modelList.add(buildMoveToOtherWindowItem());

        // Manage windows
        if (MultiWindowUtils.shouldShowManageWindowsMenu()) modelList.add(buildManageWindowsItem());

        // Divider
        AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Open History
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
            modelList.add(mHistoryItemBuilder.buildHistoryItem(shouldShowIconBeforeItem));
        }

        boolean isPageInfoItemShown = shouldShowPageInfoItem();

        // Quick Delete
        if (shouldShowQuickDeleteItem()) {
            modelList.add(buildQuickDeleteItem());
            if (!isPageInfoItemShown) {
                AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.quick_delete_divider_line_id);
            }
        }

        // Page info
        if (isPageInfoItemShown) {
            modelList.add(buildPageInfoItem(currentTab, shouldShowIconBeforeItem));
            AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.page_info_divider_line_id);
        }

        // Homepage
        if (currentTab != null && HomepageManager.getInstance().shouldShowHomepageMenuItem()) {
            modelList.add(buildHomepageItem());
        }

        // Downloads
        modelList.add(buildDownloadsItem());

        // Bookmarks
        modelList.add(mBookmarksItemBuilder.buildBookmarksItem(shouldShowIconBeforeItem));

        // Recent Tabs
        if (mHistoryItemBuilder.shouldShowRecentTabsItem()) {
            modelList.add(mHistoryItemBuilder.buildRecentTabsItem(shouldShowIconBeforeItem));
        }

        // Extensions
        if (shouldShowExtensionsItem()) {
            modelList.add(buildExtensionsMenuItem(shouldShowIconBeforeItem));
        }

        // Divider
        modelList.add(
                new ListItem(
                        AppMenuHandler.AppMenuItemType.DIVIDER,
                        AppMenuItemUtils.buildModelForDivider(R.id.divider_line_id)));

        // Page Zoom
        // Disable page zoom menu item on Reading Mode pages.
        if (shouldShowPageZoomItem(currentTab) && !isReaderModeShowing(currentTab)) {
            modelList.add(buildPageZoomItem(currentTab));
            // Divider
            modelList.add(
                    new ListItem(
                            AppMenuHandler.AppMenuItemType.DIVIDER,
                            AppMenuItemUtils.buildModelForDivider(R.id.divider_line_id)));
        }

        // Share
        if (ShareUtils.shouldEnableShare(currentTab)) {
            modelList.add(buildShareListItem(shouldShowIconBeforeItem));
        }

        // Download Page
        if (shouldShowDownloadPageMenuItem(currentTab)) {
            modelList.add(mSaveAndShareItemBuilder.buildDownloadPageItem(shouldShowIconBeforeItem));
        }

        // Print
        if (shouldShowPrintItem(currentTab)) {
            modelList.add(buildPrintItem(currentTab));
        }

        // Price Tracking (enable / disable)
        ListItem priceTrackingItem =
                maybeBuildPriceTrackingListItem(currentTab, shouldShowIconBeforeItem);
        if (priceTrackingItem != null) modelList.add(priceTrackingItem);

        // Glic
        ListItem openGlicItem = maybeBuildOpenGlicItem(currentTab);
        if (openGlicItem != null) modelList.add(openGlicItem);

        // Find in page
        if (shouldShowFindInPageItem(currentTab)) modelList.add(buildFindInPageItem(currentTab));

        // Lens Overlay
        if (shouldShowLensOverlayItem(currentTab)) modelList.add(buildLensOverlayItem(currentTab));

        // Translate
        if (shouldShowTranslateMenuItem(currentTab)) {
            modelList.add(buildTranslateMenuItem(currentTab, shouldShowIconBeforeItem));
        }

        // Readaloud
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            observeAndMaybeAddReadAloud(modelList, currentTab);
        }

        // Reader mode
        if (mMoreToolsItemBuilder.shouldShowReaderModeItem(currentTab)) {
            modelList.add(buildReaderModeItem(currentTab, shouldShowIconBeforeItem));
        }

        // Open with ...
        if (shouldShowOpenWithItem(currentTab)) {
            modelList.add(buildOpenWithItem(currentTab, shouldShowIconBeforeItem));
        }

        // Universal Install / Open Web APK
        if (shouldShowHomeScreenMenuItem(
                isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)) {
            assert currentTab != null;
            modelList.add(buildAddToHomescreenListItem(currentTab, shouldShowIconBeforeItem));
        }

        // Open in App
        if (shouldShowOpenInAppItem()) {
            modelList.add(buildOpenInAppItem());
        }

        // RDS
        ListItem rdsListItem =
                maybeBuildRequestDesktopSiteListItem(
                        currentTab, isNativePage, shouldShowIconBeforeItem);
        if (rdsListItem != null) modelList.add(rdsListItem);

        // Auto Dark
        if (shouldShowAutoDarkItem(currentTab, isNativePage)) {
            modelList.add(buildAutoDarkItem(currentTab, isNativePage, shouldShowIconBeforeItem));
        }

        // Paint Preview
        if (mSaveAndShareItemBuilder.shouldShowPaintPreview(isNativePage, currentTab)) {
            modelList.add(mSaveAndShareItemBuilder.buildPaintPreviewItem(shouldShowIconBeforeItem));
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
        AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Settings
        modelList.add(buildSettingsItem());

        // NTP Customizations
        if (mMoreToolsItemBuilder.shouldShowNtpCustomizations(currentTab)) {
            modelList.add(
                    mMoreToolsItemBuilder.buildNtpCustomizationsItem(shouldShowIconBeforeItem));
        }

        // Help
        modelList.add(buildHelpItem());

        // Managed by
        if (shouldShowManagedByMenuItem(currentTab)) {
            AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.managed_by_divider_line_id);
            modelList.add(buildManagedByItem(currentTab));
        }
        if (shouldShowContentFilterHelpCenterMenuItem(currentTab)) {
            AppMenuItemUtils.maybeAddDividerLine(
                    modelList, R.id.menu_item_content_filter_divider_line_id);
            modelList.add(buildContentFilterHelpCenterMenuItem(currentTab));
        }

        // Default browser promo
        if (shouldShowDefaultBrowserPromo()) {
            RecordUserAction.record("MobileMenuDefaultBrowserPromoShown");

            AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);
            modelList.add(buildDefaultBrowserPromoItem());
        }
    }

    private void populatePageModeMenuWithSubmenus(
            MVCListAdapter.ModelList modelList,
            @Nullable Tab currentTab,
            GURL url,
            boolean isNativePage,
            boolean isFileScheme,
            boolean isContentScheme) {
        boolean separateIncognitoWindow = IncognitoUtils.shouldOpenIncognitoAsWindow();
        boolean isIncognito = isIncognitoShowing();
        if (!separateIncognitoWindow || !isIncognito) {
            modelList.add(buildNewTabItem());
        }

        if (!separateIncognitoWindow || isIncognito) {
            modelList.add(buildNewIncognitoTabItem());
        }

        // Tab groups
        if (mTabGroupItemBuilder.shouldShowTabGroupsParentItem(currentTab)) {
            modelList.add(mTabGroupItemBuilder.buildTabGroupsParentItem(currentTab));
        }

        // New Window
        if (shouldShowNewWindow()) {
            modelList.add(buildNewWindowItem());
        }

        // New Incognito Window
        if (shouldShowNewIncognitoWindow()) {
            modelList.add(buildNewIncognitoWindowItem());
        }

        // Move to other window
        if (shouldShowMoveToOtherWindow()) {
            modelList.add(buildMoveToOtherWindowItem());
        }

        // Manage windows
        if (MultiWindowUtils.shouldShowManageWindowsMenu()) {
            modelList.add(buildManageWindowsItem());
        }

        AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);

        // History and autofill parent
        if (mHistoryItemBuilder.shouldShowHistoryParentItem()) {
            modelList.add(mHistoryItemBuilder.buildHistoryParentItem());
        }

        // Delete browsing data
        if (shouldShowQuickDeleteItem()) {
            modelList.add(buildQuickDeleteItem());
        }

        // Homepage
        if (currentTab != null && HomepageManager.getInstance().shouldShowHomepageMenuItem()) {
            modelList.add(buildHomepageItem());
        }

        AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Downloads
        modelList.add(buildDownloadsItem());

        // Bookmarks
        modelList.add(mBookmarksItemBuilder.buildBookmarksParentItem(currentTab));

        // Extensions
        if (shouldShowExtensionsItem()) {
            modelList.add(buildExtensionsParentItem());
        }

        // Passwords and autofill
        if (shouldShowPasswordsAndAutofillParentItem()) {
            modelList.add(buildPasswordsAndAutofillParentItem());
        }

        AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Page Zoom
        if (shouldShowPageZoomItem(currentTab) && !isReaderModeShowing(currentTab)) {
            modelList.add(buildPageZoomItem(currentTab));
            AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);
        }

        // Save and share
        if (shouldShowSaveAndPrintParentItem(
                currentTab, isNativePage, isFileScheme, isContentScheme, url)) {
            modelList.add(
                    buildSaveAndPrintParentItem(
                            currentTab, isNativePage, isFileScheme, isContentScheme, url));
        }

        // Glic
        ListItem openGlicItem = maybeBuildOpenGlicItem(currentTab);
        if (openGlicItem != null) modelList.add(openGlicItem);

        // Print
        if (shouldShowPrintItem(currentTab)) {
            modelList.add(buildPrintItem(currentTab));
        }

        // Price Tracking (enable / disable)
        boolean shouldShowIconBeforeItem = shouldShowIconBeforeItem();
        ListItem priceTrackingItem =
                maybeBuildPriceTrackingListItem(currentTab, shouldShowIconBeforeItem);
        if (priceTrackingItem != null) modelList.add(priceTrackingItem);

        // Find in page
        if (shouldShowFindInPageItem(currentTab)) modelList.add(buildFindInPageItem(currentTab));

        // Lens Overlay
        if (shouldShowLensOverlayItem(currentTab)) modelList.add(buildLensOverlayItem(currentTab));

        // Translate
        if (shouldShowTranslateMenuItem(currentTab)) {
            modelList.add(buildTranslateMenuItem(currentTab, shouldShowIconBeforeItem));
        }

        // More tools
        if (mMoreToolsItemBuilder.shouldShowMoreToolsItem(currentTab)) {
            modelList.add(buildMoreToolsItem(currentTab));
        }

        // Open with ...
        if (shouldShowOpenWithItem(currentTab)) {
            modelList.add(buildOpenWithItem(currentTab, shouldShowIconBeforeItem));
        }

        // Open in App
        if (shouldShowOpenInAppItem()) {
            modelList.add(buildOpenInAppItem());
        }

        // RDS
        ListItem rdsListItem =
                maybeBuildRequestDesktopSiteListItem(
                        currentTab, isNativePage, shouldShowIconBeforeItem);
        if (rdsListItem != null) modelList.add(rdsListItem);

        // Auto Dark
        if (shouldShowAutoDarkItem(currentTab, isNativePage)) {
            modelList.add(buildAutoDarkItem(currentTab, isNativePage, shouldShowIconBeforeItem));
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
        AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Settings
        modelList.add(buildSettingsItem());

        // Help
        modelList.add(buildHelpParentItem());

        // Managed by
        if (shouldShowManagedByMenuItem(currentTab)) {
            AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.managed_by_divider_line_id);
            modelList.add(buildManagedByItem(currentTab));
        }
        if (shouldShowContentFilterHelpCenterMenuItem(currentTab)) {
            AppMenuItemUtils.maybeAddDividerLine(
                    modelList, R.id.menu_item_content_filter_divider_line_id);
            modelList.add(buildContentFilterHelpCenterMenuItem(currentTab));
        }

        // Default browser promo menu item (entry point).
        if (shouldShowDefaultBrowserPromo()) {
            // Used to track how many people saw the promo.
            RecordUserAction.record("MobileMenuDefaultBrowserPromoShown");

            AppMenuItemUtils.maybeAddDividerLine(modelList, R.id.divider_line_id);
            modelList.add(buildDefaultBrowserPromoItem());
        }
    }

    private Runnable buildUpdateStateChangedObserver() {
        return () -> {
            MVCListAdapter.ModelList modelList = getModelList();
            if (modelList == null) {
                assert false : "ModelList should not be null";
                return;
            }
            for (ListItem listItem : modelList) {
                if (listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == R.id.update_menu_id) {
                    updateUpdateItemData(listItem.model);
                    return;
                }
            }
        };
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
        modelList.add(mTabGroupItemBuilder.buildNewTabGroupItemWithoutTab());
        modelList.add(buildCloseAllTabsItem());
        if (shouldShowSelectTabsItem()) modelList.add(buildSelectTabsItem());
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

    private ListItem buildUpdateItem() {
        assert shouldShowUpdateMenuItem();
        PropertyModel model =
                AppMenuItemUtils.populateBaseModelForTextItem(
                                new PropertyModel.Builder(UpdateMenuItemViewBinder.ALL_KEYS),
                                getAppMenuItemTheme(),
                                R.id.update_menu_id,
                                isMenuIconAtStart())
                        .with(AppMenuItemProperties.TITLE, mContext.getString(R.string.menu_update))
                        .with(
                                AppMenuItemProperties.ICON,
                                AppCompatResources.getDrawable(mContext, R.drawable.menu_update))
                        .build();
        updateUpdateItemData(model);
        return new ListItem(TabbedAppMenuItemType.UPDATE_ITEM, model);
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
        if (itemState.icon != Resources.ID_NULL) {
            icon = AppCompatResources.getDrawable(mContext, itemState.icon);
        }
        if (icon != null && itemState.iconTintId != Resources.ID_NULL) {
            DrawableCompat.setTint(icon, mContext.getColor(itemState.iconTintId));
        }
        model.set(AppMenuItemProperties.ICON, icon);
        model.set(AppMenuItemProperties.ENABLED, itemState.enabled);
    }

    private ListItem buildNewTabItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.new_tab_menu_id,
                        R.string.menu_new_tab,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_add_box_rounded_corner
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildHomepageItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.homepage_menu_id,
                        R.string.options_homepage_title,
                        shouldShowIconBeforeItem() ? R.drawable.ic_home_24dp : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private boolean isIncognitoShowing() {
        return mTabModelSelector.getCurrentModel().isIncognito();
    }

    private boolean isIncognitoReauthShowing() {
        return isIncognitoShowing()
                && (mIncognitoReauthController != null)
                && mIncognitoReauthController.isReauthPageShowing();
    }

    private ListItem buildNewIncognitoTabItem() {
        int iconRes = Resources.ID_NULL;
        if (shouldShowIconBeforeItem()) {
            iconRes =
                    IncognitoUtils.shouldOpenIncognitoAsWindow()
                            ? R.drawable.ic_add_box_rounded_corner
                            : R.drawable.ic_incognito;
        }
        PropertyModel model =
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.new_incognito_tab_menu_id,
                        R.string.menu_new_incognito_tab,
                        iconRes,
                        isMenuIconAtStart());
        model.set(
                AppMenuItemProperties.ENABLED, isIncognitoEnabled() && !isIncognitoReauthShowing());
        return new ListItem(TabbedAppMenuItemType.NEW_INCOGNITO, model);
    }

    private ListItem buildNewWindowItem() {
        assert shouldShowNewWindow();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.new_window_menu_id,
                        R.string.menu_new_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_new_window : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildNewIncognitoWindowItem() {
        assert shouldShowNewIncognitoWindow();
        PropertyModel model =
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.new_incognito_window_menu_id,
                        R.string.menu_new_incognito_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_incognito : Resources.ID_NULL,
                        isMenuIconAtStart());
        model.set(
                AppMenuItemProperties.ENABLED, isIncognitoEnabled() && !isIncognitoReauthShowing());
        return new ListItem(TabbedAppMenuItemType.NEW_INCOGNITO, model);
    }

    private ListItem buildMoveToOtherWindowItem() {
        assert shouldShowMoveToOtherWindow();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.move_to_other_window_menu_id,
                        R.string.menu_move_to_other_window,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_open_in_browser
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildManageWindowsItem() {
        assert MultiWindowUtils.shouldShowManageWindowsMenu();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.manage_all_windows_menu_id,
                        R.string.menu_manage_all_windows,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_select_window
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private boolean shouldShowPasswordsAndAutofillParentItem() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);
    }

    private ListItem buildGooglePasswordManagerItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.google_password_manager_menu_id,
                        R.string.menu_google_password_manager,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private ListItem buildPaymentsItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.payment_methods_menu_id,
                        R.string.menu_payment_methods,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private ListItem buildAddressesAndMoreItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.addresses_and_more_menu_id,
                        R.string.menu_addresses_and_more,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private ListItem buildPasswordsAndAutofillParentItem() {
        assert shouldShowPasswordsAndAutofillParentItem();

        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildGooglePasswordManagerItem());
        submenuItems.add(buildPaymentsItem());
        submenuItems.add(buildAddressesAndMoreItem());

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.passwords_and_autofill_parent_menu_id,
                        R.string.menu_passwords_and_autofill,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_password_manager_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems,
                        isMenuIconAtStart()));
    }

    private ListItem buildDownloadsItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.downloads_menu_id,
                        R.string.menu_downloads,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_download_done_24dp
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private boolean shouldShowExtensionsItem() {
        // TODO(crbug.com/422307625): Remove this check once extensions are ready for dogfooding.
        return ExtensionUi.isEnabled(getProfileFromTabModel());
    }

    private ListItem buildExtensionsParentItem() {
        assert shouldShowExtensionsItem();

        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildExtensionsMenuItem(/* showIcon= */ false));
        submenuItems.add(buildManageExtensionsItem());
        submenuItems.add(buildChromeWebstoreItem());

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.extensions_parent_menu_id,
                        R.string.menu_extensions,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_extension_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems,
                        isMenuIconAtStart()));
    }

    private ListItem buildExtensionsMenuItem(boolean showIcon) {
        assert shouldShowExtensionsItem();

        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.extensions_menu_menu_id,
                        R.string.menu_extensions_menu,
                        showIcon ? R.drawable.ic_extension_24dp : Resources.ID_NULL,
                        isMenuIconAtStart()),
                showIcon);
    }

    private ListItem buildManageExtensionsItem() {
        assert shouldShowExtensionsItem();

        // The id {@code R.id.extensions_menu_id} is used for both when this flag is enabled and
        // disabled but in different context.
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);

        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.manage_extensions_menu_id,
                        R.string.menu_manage_extensions,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private ListItem buildChromeWebstoreItem() {
        assert shouldShowExtensionsItem();
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.extensions_webstore_menu_id,
                        R.string.menu_chrome_webstore,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private boolean shouldShowSaveAndPrintParentItem(
            @Nullable Tab currentTab,
            boolean isNativePage,
            boolean isFileScheme,
            boolean isContentScheme,
            GURL url) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            return false;
        }

        if (ShareUtils.shouldEnableShare(currentTab)) {
            return true;
        }

        if (shouldShowDownloadPageMenuItem(currentTab)) {
            return true;
        }

        if (shouldShowHomeScreenMenuItem(
                isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)) {
            return true;
        }

        if (mSaveAndShareItemBuilder.shouldShowPaintPreview(isNativePage, currentTab)) {
            return true;
        }

        return false;
    }

    private ListItem buildSaveAndPrintParentItem(
            @Nullable Tab currentTab,
            boolean isNativePage,
            boolean isFileScheme,
            boolean isContentScheme,
            GURL url) {
        assert shouldShowSaveAndPrintParentItem(
                currentTab, isNativePage, isFileScheme, isContentScheme, url);

        List<ListItem> submenuItems = new ArrayList<>();

        if (ShareUtils.shouldEnableShare(currentTab)) {
            submenuItems.add(buildShareListItem(/* showIcon= */ false));
            submenuItems.add(mSaveAndShareItemBuilder.buildCopyLinkItem());
            submenuItems.add(mSaveAndShareItemBuilder.buildSendToDevicesItem());
            submenuItems.add(mSaveAndShareItemBuilder.buildShareQrCodeItem());
        }

        if (shouldShowDownloadPageMenuItem(currentTab)
                || shouldShowHomeScreenMenuItem(
                        isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)
                || mSaveAndShareItemBuilder.shouldShowPaintPreview(isNativePage, currentTab)) {
            AppMenuItemUtils.maybeAddDividerLine(submenuItems, R.id.divider_line_id);
        }

        if (shouldShowDownloadPageMenuItem(currentTab)) {
            submenuItems.add(mSaveAndShareItemBuilder.buildDownloadPageItem(/* showIcon= */ false));
        }

        if (shouldShowHomeScreenMenuItem(
                isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)) {
            assert currentTab != null;
            submenuItems.add(buildAddToHomescreenListItem(currentTab, /* showIcon= */ false));
        }

        if (mSaveAndShareItemBuilder.shouldShowPaintPreview(isNativePage, currentTab)) {
            submenuItems.add(mSaveAndShareItemBuilder.buildPaintPreviewItem(/* showIcon= */ false));
        }

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.save_and_share_parent_menu_id,
                        R.string.menu_save_and_share,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_share_white_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems,
                        isMenuIconAtStart()));
    }

    /** Determines whether the "Print" menu item should be shown for a given tab. */
    @Contract("null -> false")
    private boolean shouldShowPrintItem(@Nullable Tab currentTab) {
        // A tab must exist to print from it.
        if (currentTab == null) {
            return false;
        }

        // Check if sharing (which includes printing) is generally enabled for this tab's content.
        boolean canShareTab = ShareUtils.shouldEnableShare(currentTab);
        if (!canShareTab) {
            return false;
        }

        // Check if printing is specifically enabled in user preferences for the current profile.
        Profile profile = currentTab.getProfile();
        boolean isPrintingEnabled = UserPrefs.get(profile).getBoolean(Pref.PRINTING_ENABLED);
        if (!isPrintingEnabled) {
            return false;
        }

        // The print functionality is enabled if:
        // 1. The device is running Desktop Android, OR
        // 2. The current tab is a PDF page.
        NativePage nativePage = currentTab.getNativePage();
        boolean isPdf = nativePage != null && nativePage.isPdf();
        return DeviceInfo.isDesktop() || isPdf;
    }

    private ListItem buildPrintItem(Tab currentTab) {
        assert shouldShowPrintItem(currentTab);
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.print_id,
                        R.string.menu_print,
                        shouldShowIconBeforeItem() ? R.drawable.sharing_print : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildMoreToolsItem(@Nullable Tab currentTab) {
        assert mMoreToolsItemBuilder.shouldShowMoreToolsItem(currentTab);

        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();

                    ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
                    if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                            && readAloudController != null
                            && readAloudController.isReadable(currentTab)) {
                        submenuItems.add(
                                mMoreToolsItemBuilder.buildReadAloudSubmenuItem(
                                        /* showIcon= */ false));
                    }

                    if (mMoreToolsItemBuilder.shouldShowReaderModeItem(currentTab)) {
                        submenuItems.add(buildReaderModeItem(currentTab, /* showIcon= */ false));
                    }

                    AppMenuItemUtils.maybeAddDividerLine(submenuItems, R.id.divider_line_id);

                    if (mMoreToolsItemBuilder.shouldShowNameWindowItem()) {
                        submenuItems.add(
                                mMoreToolsItemBuilder.buildNameWindowItem(/* showIcon= */ false));
                    }

                    if (mMoreToolsItemBuilder.shouldShowTabLayoutToggleItem()) {
                        submenuItems.add(
                                mMoreToolsItemBuilder.buildTabLayoutToggleItem(
                                        /* showIcon= */ false));
                    }

                    if (mMoreToolsItemBuilder.shouldShowNtpCustomizations(currentTab)) {
                        submenuItems.add(
                                mMoreToolsItemBuilder.buildNtpCustomizationsItem(
                                        /* showIcon= */ false));
                    }

                    AppMenuItemUtils.maybeAddDividerLine(submenuItems, R.id.divider_line_id);

                    if (shouldShowPageInfoItem()) {
                        submenuItems.add(buildPageInfoItem(currentTab, /* showIcon= */ false));
                    }

                    if (mMoreToolsItemBuilder.shouldShowTaskManagerItem()) {
                        submenuItems.add(mMoreToolsItemBuilder.buildTaskManagerItem());
                    }

                    if (mMoreToolsItemBuilder.shouldShowDevToolsItem(currentTab)) {
                        submenuItems.add(mMoreToolsItemBuilder.buildDevToolsItem());
                    }

                    if (!submenuItems.isEmpty()
                            && submenuItems.get(submenuItems.size() - 1).type
                                    == AppMenuHandler.AppMenuItemType.DIVIDER) {
                        submenuItems.remove(submenuItems.size() - 1);
                    }

                    return submenuItems;
                };

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.more_tools_menu_id,
                        R.string.menu_more_tools,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_more_tools_24dp
                                : Resources.ID_NULL,
                        submenuItemsSupplier,
                        isMenuIconAtStart()));
    }

    @Contract("null -> false")
    private boolean shouldShowGetImageDescriptionsItem(@Nullable Tab currentTab) {
        return currentTab != null
                && shouldShowWebContentsDependentMenuItem(currentTab)
                && ImageDescriptionsController.getInstance().shouldShowImageDescriptionsMenuItem();
    }

    private ListItem buildGetImageDescriptionsItem(Tab currentTab) {
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

        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.get_image_descriptions_id,
                        titleId,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_image_descriptions
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildCloseAllTabsItem() {
        final PropertyModel model;
        if (isIncognitoShowing()) {
            model =
                    AppMenuItemUtils.buildModelForStandardMenuItem(
                            mContext,
                            getAppMenuItemTheme(),
                            R.id.close_all_incognito_tabs_menu_id,
                            R.string.menu_close_all_incognito_tabs,
                            shouldShowIconBeforeItem()
                                    ? R.drawable.ic_close_all_tabs
                                    : Resources.ID_NULL,
                            isMenuIconAtStart());
            model.set(
                    AppMenuItemProperties.ENABLED, mTabModelSelector.getModel(true).getCount() > 0);
        } else {
            model =
                    AppMenuItemUtils.buildModelForStandardMenuItem(
                            mContext,
                            getAppMenuItemTheme(),
                            R.id.close_all_tabs_menu_id,
                            R.string.menu_close_all_tabs,
                            shouldShowIconBeforeItem()
                                    ? R.drawable.btn_close_white
                                    : Resources.ID_NULL,
                            isMenuIconAtStart());
            model.set(AppMenuItemProperties.ENABLED, mTabModelSelector.getTotalTabCount() > 0);
        }
        return new ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private boolean shouldShowSelectTabsItem() {
        HubManager hubManager = mHubManagerSupplier.get();
        if (hubManager == null) return false;

        Pane focusedPane = hubManager.getPaneManager().getFocusedPaneSupplier().get();
        if (focusedPane == null) return false;

        return focusedPane.getPaneId() == PaneId.TAB_SWITCHER
                || focusedPane.getPaneId() == PaneId.INCOGNITO_TAB_SWITCHER;
    }

    private ListItem buildSelectTabsItem() {
        PropertyModel model =
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.menu_select_tabs,
                        R.string.menu_select_tabs,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_select_check_box_24dp
                                : Resources.ID_NULL,
                        isMenuIconAtStart());
        boolean isEnabled =
                !isIncognitoReauthShowing()
                        && mTabModelSelector.isTabStateInitialized()
                        && mTabModelSelector.getCurrentModel().getCount() != 0;
        model.set(AppMenuItemProperties.ENABLED, isEnabled);

        return new ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private ListItem buildSettingsItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.preferences_id,
                        R.string.menu_settings,
                        shouldShowIconBeforeItem() ? R.drawable.settings_cog : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    @Contract("null -> false")
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

    private ListItem buildListenToFeedItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.listen_to_feed_id,
                        R.string.menu_listen_to_feed,
                        R.drawable.ic_play_circle,
                        isMenuIconAtStart()));
    }

    private ListItem buildHelpItem() {
        int helpString = HelpAndFeedbackLauncher.getHelpMenuStringRes();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.help_id,
                        helpString,
                        shouldShowIconBeforeItem() ? R.drawable.ic_help_24dp : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildHelpParentItem() {
        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildAboutChromeItem());
        submenuItems.add(buildHelpCenterItem());
        if (FeedbackPolicyManager.getInstance().isUserFeedbackAllowed()) {
            submenuItems.add(buildReportIssueItem());
        }

        int helpString = HelpAndFeedbackLauncher.getHelpMenuStringRes();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.help_parent_menu_id,
                        helpString,
                        shouldShowIconBeforeItem() ? R.drawable.ic_help_24dp : Resources.ID_NULL,
                        () -> submenuItems,
                        isMenuIconAtStart()));
    }

    private ListItem buildHelpCenterItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.help_id,
                        R.string.menu_help_center,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private ListItem buildReportIssueItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.report_issue_menu_id,
                        R.string.menu_report_issue,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private ListItem buildAboutChromeItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.about_chrome_menu_id,
                        R.string.menu_about_chrome,
                        Resources.ID_NULL,
                        isMenuIconAtStart()),
                /* showIcon= */ false);
    }

    private boolean shouldShowQuickDeleteItem() {
        return !isIncognitoShowing();
    }

    private ListItem buildQuickDeleteItem() {
        assert shouldShowQuickDeleteItem();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.quick_delete_menu_id,
                        R.string.menu_quick_delete,
                        shouldShowIconBeforeItem()
                                ? R.drawable.material_ic_delete_24dp
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    @Override
    public boolean shouldShowIconRow() {
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

    private ListItem buildFindInPageItem(@Nullable Tab currentTab) {
        assert shouldShowFindInPageItem(currentTab);
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.find_in_page_id,
                        R.string.menu_find_in_page,
                        shouldShowIconBeforeItem() ? R.drawable.ic_find_in_page : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private boolean shouldShowLensOverlayItem(@Nullable Tab currentTab) {
        return LensOverlayTabHelper.shouldShowLensOverlay(currentTab);
    }

    private MVCListAdapter.ListItem buildLensOverlayItem(@Nullable Tab currentTab) {
        assert shouldShowLensOverlayItem(currentTab);
        PropertyModel model =
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.lens_overlay_menu_id,
                        R.string.menu_search_tab_with_google_lens,
                        shouldShowIconBeforeItem()
                                ? R.drawable.lens_camera_icon
                                : Resources.ID_NULL,
                        isMenuIconAtStart());

        // Disable the item if the overlay is already showing.
        model.set(
                AppMenuItemProperties.ENABLED, !LensOverlayTabHelper.isOverlayShowing(currentTab));

        return new MVCListAdapter.ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private boolean shouldShowDefaultBrowserPromo() {
        return DefaultBrowserPromoUtils.getInstance().shouldShowAppMenuItemEntryPoint()
                && ChromeFeatureList.sDefaultBrowserPromoEntryPointShowAppMenu.getValue();
    }

    private ListItem buildDefaultBrowserPromoItem() {
        assert shouldShowDefaultBrowserPromo();
        PropertyModel model =
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.default_browser_promo_menu_id,
                        R.string.make_chrome_default,
                        Resources.ID_NULL,
                        isMenuIconAtStart());

        // Make the Chrome logo environment specific (Canary logo for Canary, etc.).
        model.set(
                AppMenuItemProperties.ICON,
                AppCompatResources.getDrawable(mContext, R.mipmap.app_icon));

        // Disable the grey default tint for this particular icon.
        model.set(AppMenuItemProperties.ICON_NO_TINT, true);

        return new ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private @Nullable ListItem maybeBuildOpenGlicItem(@Nullable Tab currentTab) {
        if (currentTab == null
                || currentTab.isIncognito()
                || currentTab.getWebContents() == null
                || !GlicEnabling.isEnabledForProfile(currentTab.getProfile())) {
            return null;
        }
        // Only enforce width constraints if side panel feature is enabled on the device.
        // TODO(crbug.com/519680563): Remove this side panel check once bottom sheet enabled on LFF.
        if (AndroidSidePanelEnabledFn.isEnabled()) {
            SideUiStateProvider sideUiStateProvider = mSideUiStateProviderSupplier.get();
            assert sideUiStateProvider != null;
            if (!sideUiStateProvider.canShowSideUi(SideUiId.SIDE_PANEL)) {
                return null;
            }
        }

        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.glic_menu_id,
                        R.string.glic_button_entrypoint_ask_gemini_label,
                        shouldShowIconBeforeItem() ? R.drawable.ic_spark_24dp : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    /**
     * @return Whether the "New window" menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowNewWindow() {
        // Hide the menu on automotive devices.
        if (DeviceInfo.isAutomotive()) return false;

        if (isMultiInstanceEnabled()) {
            // Hide the menu if we already have the maximum number of windows.
            if (MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE)
                    >= MultiWindowUtils.getMaxInstances()) return false;

            // On phones, show the menu only when in split-screen, with a single instance
            // running on the foreground.
            return isTabletSizeScreen()
                    || (!mMultiWindowModeStateDispatcher.isChromeRunningInAdjacentWindow()
                            && (mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                                    || mMultiWindowModeStateDispatcher.isInMultiDisplayMode()));
        } else {
            if (mMultiWindowModeStateDispatcher.isMultiInstanceRunning()) return false;
            return mMultiWindowModeStateDispatcher.isInMultiWindowMode()
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
        if (!isMultiInstanceEnabled() && shouldShowNewWindow()) return false;
        return mMultiWindowModeStateDispatcher.isMoveToOtherWindowSupported(mTabModelSelector);
    }

    @VisibleForTesting
    @Contract("null -> false")
    protected boolean shouldShowManagedByMenuItem(@Nullable Tab currentTab) {
        return currentTab != null && ManagedBrowserUtils.isBrowserManaged(currentTab.getProfile());
    }

    @Contract("null -> false")
    protected boolean shouldShowContentFilterHelpCenterMenuItem(@Nullable Tab currentTab) {
        return currentTab != null
                && SupervisedUserServiceBridge.isSupervisedLocally(currentTab.getProfile());
    }

    private ListItem buildManagedByItem(Tab currentTab) {
        assert shouldShowManagedByMenuItem(currentTab);
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.managed_by_menu_id,
                        R.string.managed_browser,
                        shouldShowIconBeforeItem() ? R.drawable.ic_domain : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    private ListItem buildContentFilterHelpCenterMenuItem(Tab currentTab) {
        assert shouldShowContentFilterHelpCenterMenuItem(currentTab);
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        getAppMenuItemTheme(),
                        R.id.menu_item_content_filter_help_center_id,
                        R.string.menu_item_content_filter_help_center_link,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_account_child_20dp
                                : Resources.ID_NULL,
                        isMenuIconAtStart()));
    }

    @Override
    public boolean shouldShowIconBeforeItem() {
        return true;
    }

    @Override
    public void onMenuDismissed() {
        super.onMenuDismissed();
        RecentlyClosedEntriesManager manager = mRecentlyClosedEntriesManagerSupplier.get();
        if (manager != null) {
            manager.clearTabListCache();
        }
        if (mUpdateMenuItemVisible) {
            UpdateMenuItemHelper updateHelper =
                    UpdateMenuItemHelper.getInstance(getProfileFromTabModel());
            updateHelper.onMenuDismissed();
            updateHelper.unregisterObserver(assumeNonNull(mUpdateStateChangeObserver));
            mUpdateMenuItemVisible = false;
            mUpdateStateChangeObserver = null;
        }
    }

    @Override
    public void onMenuShown() {
        super.onMenuShown();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            // TODO(crbug.com/521223427): Implement dynamic updates so that we don't
            // have to rely on timing to load the {@link BookmarkModel} and {@link
            // HeadlessTabModel}.
            BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
            if (bookmarkModel != null && !bookmarkModel.isBookmarkModelLoaded()) {
                bookmarkModel.finishLoadingBookmarkModel(() -> {});
            }
            RecentlyClosedEntriesManager manager = mRecentlyClosedEntriesManagerSupplier.get();
            if (manager != null) {
                manager.updateRecentlyClosedEntries();
            }
        }
    }

    @Override
    protected void observeAndMaybeAddReadAloud(ModelList modelList, @Nullable Tab currentTab) {
        // TODO(crbug.com/521223427): This is not ideal. When SUBMENUS_IN_APP_MENU is enabled,
        // we suppress the main menu insertion and rely on querying ReadAloudController live
        // in the "More tools" submenu Supplier. Ideally, we should implement a mechanism to
        // dynamically update the item visibility in the submenu, rather than requiring the user to
        // reopen the App Menu.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            super.observeAndMaybeAddReadAloud(modelList, currentTab);
        }
    }

    private Profile getProfileFromTabModel() {
        var profile = mTabModelSelector.getModel(false).getProfile();
        assert profile != null;
        return profile;
    }

    public void setImageFetcherForTesting(BookmarkImageFetcher imageFetcher) {
        mBookmarksItemBuilder.setImageFetcherForTesting(imageFetcher);
    }

    public void setSaveAndShareItemBuilderForTesting(SaveAndShareItemBuilder builder) {
        mSaveAndShareItemBuilder = builder;
    }

    public SaveAndShareItemBuilder getSaveAndShareItemBuilderForTesting() {
        return mSaveAndShareItemBuilder;
    }
}
