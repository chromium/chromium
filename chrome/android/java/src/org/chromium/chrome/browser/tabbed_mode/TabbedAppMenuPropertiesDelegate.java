// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.devtools.DevToolsWindowAndroid;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.feed.FeedFeatures;
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
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedGroup;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.ntp.TitleUtil;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.open_in_app.OpenInAppMenuItemProvider;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.share.ShareUtils;
import org.chromium.chrome.browser.supervised_user.SupervisedUserServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.toolbar.top.ToolbarUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBookmarkItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuRecentEntryItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTabItemProperties;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.lens.LensOverlayTabHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.accessibility.PageZoomManager;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.function.BiFunction;
import java.util.function.Supplier;

/** An {@link AppMenuPropertiesDelegateImpl} for ChromeTabbedActivity. */
@NullMarked
public class TabbedAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {

    public static final int MAX_RECENT_ENTRIES_TO_SHOW = 8;

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

    private boolean mUpdateMenuItemVisible;

    /**
     * This is non null for the case of ChromeTabbedActivity when the corresponding {@link
     * CallbackController} has been fired.
     */
    private @Nullable IncognitoReauthController mIncognitoReauthController;

    private @Nullable Runnable mUpdateStateChangeObserver;

    private final CallbackController mIncognitoReauthCallbackController = new CallbackController();

    private final OneshotSupplier<HubManager> mHubManagerSupplier;

    private @Nullable BookmarkImageFetcher mImageFetcher;
    private @Nullable FaviconHelper mFaviconHelper;
    private final FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
    private final RoundedIconGenerator mRoundedIconGenerator;
    private final Supplier<RecentlyClosedEntriesManager> mRecentlyClosedEntriesManagerSupplier;

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
            Supplier<RecentlyClosedEntriesManager> recentlyClosedEntriesManagerSupplier) {
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

        incognitoReauthControllerOneshotSupplier.onAvailable(
                mIncognitoReauthCallbackController.makeCancelable(
                        incognitoReauthController -> {
                            mIncognitoReauthController = incognitoReauthController;
                        }));
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

    @Override
    public void destroy() {
        super.destroy();
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
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
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            if (shouldShowAddToGroup()) {
                modelList.add(buildAddToGroupItem(currentTab));
            }
        }

        // New Window
        if (shouldShowNewWindow()) modelList.add(buildNewWindowItem());

        // New Incognito Window
        if (shouldShowNewIncognitoWindow()) modelList.add(buildNewIncognitoWindowItem());

        // Move to other window
        if (shouldShowMoveToOtherWindow()) modelList.add(buildMoveToOtherWindowItem());

        // Manage windows
        if (MultiWindowUtils.shouldShowManageWindowsMenu()) modelList.add(buildManageWindowsItem());

        // Tab groups
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            if (shouldShowTabGroupsParentItem(currentTab)) {
                modelList.add(buildTabGroupsParentItem(currentTab));
            }
        }

        // Divider
        maybeAddDividerLine(modelList, R.id.divider_line_id);

        // Passwords and autofill parent
        if (shouldShowPasswordsAndAutofillParentItem()) {
            modelList.add(buildPasswordsAndAutofillParentItem());
        }

        // History parent
        if (shouldShowHistoryParentItem()) {
            modelList.add(buildHistoryParentItem());
        }

        // Open History
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing())) {
            modelList.add(buildHistoryItem());
        }

        boolean isPageInfoItemShown = shouldShowPageInfoItem();

        // Quick Delete
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && shouldShowQuickDeleteItem()) {
            modelList.add(buildQuickDeleteItem());
            if (!isPageInfoItemShown) {
                maybeAddDividerLine(modelList, R.id.quick_delete_divider_line_id);
            }
        }

        // Page info
        if (isPageInfoItemShown) {
            modelList.add(buildPageInfoItem(currentTab));
            maybeAddDividerLine(modelList, R.id.page_info_divider_line_id);
        }

        // Homepage
        if (currentTab != null && HomepageManager.getInstance().shouldShowHomepageMenuItem()) {
            modelList.add(buildHomepageItem());
        }

        // Downloads
        modelList.add(buildDownloadsItem());

        // Bookmarks
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            modelList.add(buildBookmarksParentItem());
        } else {
            modelList.add(buildBookmarksItem());
        }

        // Recent Tabs
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && shouldShowRecentTabsItem()) {
            modelList.add(buildRecentTabsItem());
        }

        // Extensions
        if (shouldShowExtensionsItem()) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
                modelList.add(buildExtensionsParentItem());
            } else {
                modelList.add(buildExtensionsMenuItem());
            }
        }

        // Divider
        modelList.add(
                new ListItem(
                        AppMenuHandler.AppMenuItemType.DIVIDER,
                        buildModelForDivider(R.id.divider_line_id)));

        // Page Zoom
        // Disable page zoom menu item on Reading Mode pages.
        if (shouldShowPageZoomItem(currentTab) && !isReaderModeShowing(currentTab)) {
            modelList.add(buildPageZoomItem(currentTab));
            // Divider
            modelList.add(
                    new ListItem(
                            AppMenuHandler.AppMenuItemType.DIVIDER,
                            buildModelForDivider(R.id.divider_line_id)));
        }

        // Share
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && ShareUtils.shouldEnableShare(currentTab)) {
            modelList.add(buildShareListItem(shouldShowIconBeforeItem()));
        }

        // Save and print
        if (shouldShowSaveAndPrintParentItem(
                currentTab, isNativePage, isFileScheme, isContentScheme, url)) {
            modelList.add(
                    buildSaveAndPrintParentItem(
                            currentTab, isNativePage, isFileScheme, isContentScheme, url));
        }

        // Download Page
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && shouldShowDownloadPageMenuItem(currentTab)) {
            modelList.add(buildDownloadPageItem(currentTab));
        }

        // Print
        if (shouldShowPrintItem(currentTab)) {
            modelList.add(buildPrintItem(currentTab));
        }

        // Price Tracking (enable / disable)
        ListItem priceTrackingItem =
                maybeBuildPriceTrackingListItem(currentTab, shouldShowIconBeforeItem());
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
            modelList.add(buildTranslateMenuItem(currentTab, shouldShowIconBeforeItem()));
        }

        // Readaloud
        observeAndMaybeAddReadAloud(modelList, currentTab);

        // More tools
        if (shouldShowMoreToolsItem(currentTab)) {
            modelList.add(buildMoreToolsItem(currentTab));
        }

        // Reader mode
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && shouldShowReaderModeItem(currentTab)) {
            modelList.add(buildReaderModeItem(currentTab));
        }

        // Open with ...
        if (shouldShowOpenWithItem(currentTab)) {
            modelList.add(buildOpenWithItem(currentTab, shouldShowIconBeforeItem()));
        }

        // Universal Install / Open Web APK
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && shouldShowHomeScreenMenuItem(
                        isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)) {
            assert currentTab != null;
            modelList.add(buildAddToHomescreenListItem(currentTab, shouldShowIconBeforeItem()));
        }

        // Open in App
        if (shouldShowOpenInAppItem()) {
            modelList.add(buildOpenInAppItem());
        }

        // RDS
        ListItem rdsListItem =
                maybeBuildRequestDesktopSiteListItem(
                        currentTab, isNativePage, shouldShowIconBeforeItem());
        if (rdsListItem != null) modelList.add(rdsListItem);

        // Auto Dark
        if (shouldShowAutoDarkItem(currentTab, isNativePage)) {
            modelList.add(buildAutoDarkItem(currentTab, isNativePage, shouldShowIconBeforeItem()));
        }

        // Paint Preview
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)
                && shouldShowPaintPreview(isNativePage, currentTab)) {
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            modelList.add(buildHelpParentItem());
        } else {
            modelList.add(buildHelpItem());
        }

        // Managed by
        if (shouldShowManagedByMenuItem(currentTab)) {
            maybeAddDividerLine(modelList, R.id.managed_by_divider_line_id);
            modelList.add(buildManagedByItem(currentTab));
        }
        if (shouldShowContentFilterHelpCenterMenuItem(currentTab)) {
            maybeAddDividerLine(modelList, R.id.menu_item_content_filter_divider_line_id);
            modelList.add(buildContentFilterHelpCenterMenuItem(currentTab));
        }

        // Default browser promo menu item (entry point).
        if (shouldShowDefaultBrowserPromo()) {
            // Used to track how many people saw the promo.
            RecordUserAction.record("MobileMenuDefaultBrowserPromoShown");

            maybeAddDividerLine(modelList, R.id.divider_line_id);
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

    private void maybeAddDividerLine(MVCListAdapter.ModelList modelList, @IdRes int id) {
        if (modelList.get(modelList.size() - 1).type == AppMenuHandler.AppMenuItemType.DIVIDER) {
            return;
        }

        modelList.add(
                new ListItem(AppMenuHandler.AppMenuItemType.DIVIDER, buildModelForDivider(id)));
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
        modelList.add(buildNewTabGroupItem());
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
                populateBaseModelForTextItem(
                                new PropertyModel.Builder(UpdateMenuItemViewBinder.ALL_KEYS),
                                R.id.update_menu_id)
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
        if (itemState.icon != 0) {
            icon = AppCompatResources.getDrawable(mContext, itemState.icon);
        }
        if (icon != null && itemState.iconTintId != 0) {
            DrawableCompat.setTint(icon, mContext.getColor(itemState.iconTintId));
        }
        model.set(AppMenuItemProperties.ICON, icon);
        model.set(AppMenuItemProperties.ENABLED, itemState.enabled);
    }

    private ListItem buildNewTabItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.new_tab_menu_id,
                        R.string.menu_new_tab,
                        shouldShowIconBeforeItem() ? R.drawable.ic_add_box_rounded_corner : 0));
    }

    private ListItem buildHomepageItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.homepage_menu_id,
                        R.string.options_homepage_title,
                        shouldShowIconBeforeItem() ? R.drawable.ic_home_24dp : 0));
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
        return new ListItem(TabbedAppMenuItemType.NEW_INCOGNITO, model);
    }

    private boolean shouldShowAddToGroup() {
        return mTabModelSelector.isTabStateInitialized();
    }

    private ListItem buildAddToGroupItem(@Nullable Tab currentTab) {
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
        return new ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model);
    }

    private boolean shouldShowTabGroupsParentItem(@Nullable Tab currentTab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            return false;
        }

        return shouldShowAddToGroup() || currentTab != null;
    }

    private ListItem buildCreateNewTabGroupItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.create_new_tab_group_menu_id,
                        R.string.menu_create_new_tab_group,
                        shouldShowIconBeforeItem() ? R.drawable.ic_library_add_24dp : 0));
    }

    private ListItem buildTabGroupsParentItem(@Nullable Tab currentTab) {
        assert shouldShowTabGroupsParentItem(currentTab);

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.tab_groups_parent_menu_id,
                        R.string.menu_tab_groups,
                        shouldShowIconBeforeItem() ? R.drawable.ic_widgets : Resources.ID_NULL,
                        () -> getTabGroupsSubmenuItems(currentTab)));
    }

    private Drawable getTabGroupDrawable(@TabGroupColorId int color) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.OVAL);
        drawable.setColor(
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, color, isIncognitoShowing()));
        int size =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        drawable.setSize(size, size);
        return drawable;
    }

    private List<ListItem> getTabGroupsSubmenuItems(@Nullable Tab currentTab) {
        List<ListItem> submenuItems = new ArrayList<>();
        if (shouldShowAddToGroup()) {
            submenuItems.add(buildAddToGroupItem(currentTab));
        }
        if (currentTab != null) {
            submenuItems.add(buildCreateNewTabGroupItem());
        }

        TabModel tabModel = mTabModelSelector.getCurrentModel();
        Set<Token> groupIds = tabModel.getAllTabGroupIds();
        if (groupIds.isEmpty()) {
            return submenuItems;
        }

        submenuItems.add(
                new ListItem(
                        AppMenuHandler.AppMenuItemType.DIVIDER,
                        buildModelForDivider(R.id.divider_line_id)));

        // TODO(crbug.com/509065807): Observe TabModel to update this while the menu is open.
        for (Token groupId : groupIds) {
            String title = tabModel.getTabGroupTitle(groupId);
            if (TextUtils.isEmpty(title)) {
                title =
                        TabGroupTitleUtils.getDefaultTitle(
                                mContext, tabModel.getTabCountForGroup(groupId));
            }

            PropertyModel model =
                    buildModelForMenuItemWithSubmenu(
                            R.id.tab_group_menu_item_id,
                            title,
                            getTabGroupDrawable(tabModel.getTabGroupColorWithFallback(groupId)),
                            () -> getTabsSubmenuItems(groupId, tabModel));
            model.set(AppMenuItemProperties.ICON_NO_TINT, true);

            submenuItems.add(
                    new ListItem(AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU, model));
        }

        return submenuItems;
    }

    private List<ListItem> getTabsSubmenuItems(Token groupId, TabModel tabModel) {
        List<ListItem> submenuItems = new ArrayList<>();
        List<Tab> tabs = tabModel.getTabsInGroup(groupId);
        for (Tab tab : tabs) {
            PropertyModel model =
                    populateBaseModelForTextItem(
                                    new PropertyModel.Builder(AppMenuTabItemProperties.ALL_KEYS),
                                    R.id.tab_group_tab_menu_item)
                            .with(AppMenuItemProperties.TITLE, tab.getTitle())
                            .with(AppMenuTabItemProperties.TAB_ID, tab.getId())
                            .with(
                                    AppMenuItemProperties.ICON_SUPPLIER,
                                    createIconSupplierForTab(
                                            tab.getUrl(),
                                            tab.getTabGroupId(),
                                            tab.isOffTheRecord(),
                                            TabFavicon.getBitmap(tab),
                                            /* fallbackToHost= */ false))
                            .build();
            submenuItems.add(new ListItem(AppMenuHandler.AppMenuItemType.TAB, model));
        }
        return submenuItems;
    }

    private LazyOneshotSupplier<Drawable> createIconSupplierForTab(
            GURL faviconUrl,
            @Nullable Token tabGroupId,
            boolean isOffTheRecord,
            @Nullable Bitmap cachedFavicon,
            boolean fallbackToHost) {
        return new LazyOneshotSupplierImpl<>() {
            @Override
            public void doSet() {
                int faviconDisplaySize =
                        mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);

                if (cachedFavicon != null) {
                    set(
                            FaviconUtils.getIconDrawableWithFilter(
                                    cachedFavicon,
                                    faviconUrl,
                                    mRoundedIconGenerator,
                                    mDefaultFaviconHelper,
                                    mContext,
                                    faviconDisplaySize));
                    return;
                }

                FaviconHelper.FaviconImageCallback faviconCallback =
                        (image, iconUrl) -> {
                            set(
                                    FaviconUtils.getIconDrawableWithFilter(
                                            image,
                                            faviconUrl,
                                            mRoundedIconGenerator,
                                            mDefaultFaviconHelper,
                                            mContext,
                                            faviconDisplaySize));
                        };

                Profile profile = getProfileFromTabModel();
                if (tabGroupId != null && !isOffTheRecord) {
                    getFaviconHelper()
                            .getForeignFaviconImageForURL(
                                    profile,
                                    faviconUrl,
                                    faviconDisplaySize,
                                    fallbackToHost,
                                    faviconCallback);
                } else {
                    getFaviconHelper()
                            .getLocalFaviconImageForURL(
                                    profile,
                                    faviconUrl,
                                    faviconDisplaySize,
                                    fallbackToHost,
                                    faviconCallback);
                }
            }
        };
    }

    private ListItem buildNewWindowItem() {
        assert shouldShowNewWindow();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.new_window_menu_id,
                        R.string.menu_new_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_new_window : 0));
    }

    private ListItem buildNewIncognitoWindowItem() {
        assert shouldShowNewIncognitoWindow();
        PropertyModel model =
                buildModelForStandardMenuItem(
                        R.id.new_incognito_window_menu_id,
                        R.string.menu_new_incognito_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_incognito : 0);
        model.set(
                AppMenuItemProperties.ENABLED, isIncognitoEnabled() && !isIncognitoReauthShowing());
        return new ListItem(TabbedAppMenuItemType.NEW_INCOGNITO, model);
    }

    private ListItem buildMoveToOtherWindowItem() {
        assert shouldShowMoveToOtherWindow();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.move_to_other_window_menu_id,
                        R.string.menu_move_to_other_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_open_in_browser : 0));
    }

    private ListItem buildManageWindowsItem() {
        assert MultiWindowUtils.shouldShowManageWindowsMenu();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.manage_all_windows_menu_id,
                        R.string.menu_manage_all_windows,
                        shouldShowIconBeforeItem() ? R.drawable.ic_select_window : 0));
    }

    private boolean shouldShowPasswordsAndAutofillParentItem() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);
    }

    private ListItem buildGooglePasswordManagerItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.google_password_manager_menu_id,
                        R.string.menu_google_password_manager,
                        shouldShowIconBeforeItem() ? R.drawable.ic_password_manager_24dp : 0));
    }

    private ListItem buildPaymentsItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.payment_methods_menu_id,
                        R.string.menu_payment_methods,
                        shouldShowIconBeforeItem() ? R.drawable.ic_credit_card_24dp : 0));
    }

    private ListItem buildAddressesAndMoreItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.addresses_and_more_menu_id,
                        R.string.menu_addresses_and_more,
                        shouldShowIconBeforeItem() ? R.drawable.ic_address_24dp : 0));
    }

    private ListItem buildPasswordsAndAutofillParentItem() {
        assert shouldShowPasswordsAndAutofillParentItem();

        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildGooglePasswordManagerItem());
        submenuItems.add(buildPaymentsItem());
        submenuItems.add(buildAddressesAndMoreItem());

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.passwords_and_autofill_parent_menu_id,
                        R.string.menu_passwords_and_autofill,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_password_manager_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems));
    }

    private boolean shouldShowHistoryParentItem() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            return false;
        }

        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
            return true;
        }

        if (shouldShowRecentTabsItem()) {
            return true;
        }

        if (shouldShowQuickDeleteItem()) {
            return true;
        }

        return false;
    }

    private ListItem buildHistoryParentItem() {
        assert shouldShowHistoryParentItem();

        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();
                    if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
                        submenuItems.add(buildHistoryItem());
                    }

                    if (shouldShowRecentTabsItem()) {
                        submenuItems.add(buildRecentTabsItem());
                    }

                    if (shouldShowQuickDeleteItem()) {
                        submenuItems.add(buildQuickDeleteItem());
                    }

                    List<ListItem> recentEntries = getRecentEntryMenuItemList();
                    if (!recentEntries.isEmpty()) {
                        submenuItems.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.DIVIDER,
                                        buildModelForDivider(R.id.divider_line_id)));
                        submenuItems.addAll(recentEntries);
                    }

                    return submenuItems;
                };

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.history_parent_menu_id,
                        R.string.menu_history,
                        shouldShowIconBeforeItem() ? R.drawable.ic_history_24dp : Resources.ID_NULL,
                        submenuItemsSupplier));
    }

    private List<ListItem> getRecentEntryMenuItemList() {
        List<ListItem> items = new ArrayList<>();
        RecentlyClosedEntriesManager manager = mRecentlyClosedEntriesManagerSupplier.get();
        assert manager != null;

        manager.updateRecentlyClosedEntries();

        // TODO(crbug.com/509065810): Support updating the menu items dynamically when the
        // recently closed entries list changes while the menu is open.
        int count = 0;
        for (RecentlyClosedEntry entry : manager.getRecentlyClosedEntries()) {
            if (count >= MAX_RECENT_ENTRIES_TO_SHOW) {
                break;
            }

            if (entry instanceof RecentlyClosedTab tab) {
                items.add(
                        buildRecentEntryMenuItem(
                                entry,
                                TitleUtil.getTitleForDisplay(tab.getTitle(), tab.getUrl()),
                                createIconSupplierForTab(
                                        tab.getUrl(),
                                        tab.getTabGroupId(),
                                        // Recently closed tabs are not tracked for incognito.
                                        /* isOffTheRecord= */ false,
                                        // No live Tab object is available to get a cached favicon.
                                        /* cachedFavicon= */ null,
                                        /* fallbackToHost= */ false)));
                count++;
            } else if (entry instanceof RecentlyClosedWindow window) {
                items.add(buildClosedWindowMenuItem(window));
                manager.preFetchTabsForWindow(window);
                count++;
            } else if (entry instanceof RecentlyClosedGroup group) {
                items.add(buildClosedGroupMenuItem(group));
                count++;
            }

            // TODO(crbug.com/509065810): Support other bulk closures.
        }
        return items;
    }

    private String getRecentEntrySubmenuTitle(@Nullable String title, int tabCount) {
        String tabsText =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.recent_tabs_group_closure_without_title,
                                tabCount,
                                tabCount);

        return TextUtils.isEmpty(title)
                ? tabsText
                : mContext.getString(R.string.menu_window_title_with_tab_count, title, tabsText);
    }

    private ListItem buildClosedWindowMenuItem(RecentlyClosedWindow window) {
        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();
                    submenuItems.add(buildRestoreWindowMenuItem(window));

                    RecentlyClosedEntriesManager manager =
                            mRecentlyClosedEntriesManagerSupplier.get();
                    assert manager != null;

                    List<RecentlyClosedTab> tabs = manager.getTabsForClosedWindow(window);
                    if (!tabs.isEmpty()) {
                        submenuItems.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.DIVIDER,
                                        buildModelForDivider(R.id.divider_line_id)));
                        for (RecentlyClosedTab tab : tabs) {
                            submenuItems.add(buildClosedWindowTabMenuItem(tab));
                        }
                    }
                    return submenuItems;
                };

        PropertyModel model =
                buildModelForMenuItemWithSubmenu(
                        R.id.recent_entry_menu_item,
                        getRecentEntrySubmenuTitle(
                                window.getTitle().equals(RecentlyClosedWindow.WINDOW_DEFAULT_TITLE)
                                        ? null
                                        : window.getTitle(),
                                window.getTabCount()),
                        shouldShowIconBeforeItem() ? R.drawable.ic_window_24dp : Resources.ID_NULL,
                        submenuItemsSupplier);

        return new ListItem(AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU, model);
    }

    private ListItem buildRestoreWindowMenuItem(RecentlyClosedWindow window) {
        PropertyModel model =
                populateBaseModelForTextItem(
                                new PropertyModel.Builder(
                                        AppMenuRecentEntryItemProperties.ALL_KEYS),
                                R.id.recent_entry_window_menu_item)
                        .with(
                                AppMenuItemProperties.TITLE,
                                mContext.getString(R.string.menu_recent_entry_restore_window))
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, window)
                        .with(
                                AppMenuItemProperties.ICON,
                                AppCompatResources.getDrawable(
                                        mContext, R.drawable.ic_open_in_new_24dp))
                        .build();
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY, model);
    }

    private ListItem buildClosedWindowTabMenuItem(RecentlyClosedTab tab) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuRecentEntryItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, R.id.recent_entry_tab_menu_item)
                        .with(AppMenuItemProperties.TITLE, tab.getTitle())
                        .with(
                                AppMenuItemProperties.ICON_SUPPLIER,
                                createIconSupplierForTab(
                                        tab.getUrl(),
                                        /* tabGroupId= */ null,
                                        /* isOffTheRecord= */ false,
                                        /* cachedFavicon= */ null,
                                        /* fallbackToHost= */ false))
                        .with(AppMenuItemProperties.ICON_NO_TINT, true)
                        .with(AppMenuItemProperties.ENABLED, false)
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, tab)
                        .build();
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY, model);
    }

    private ListItem buildClosedGroupMenuItem(RecentlyClosedGroup group) {
        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();
                    submenuItems.add(buildRestoreGroupMenuItem(group));

                    List<RecentlyClosedTab> tabs = group.getTabs();
                    if (tabs.isEmpty()) {
                        return submenuItems;
                    }
                    submenuItems.add(
                            new ListItem(
                                    AppMenuHandler.AppMenuItemType.DIVIDER,
                                    buildModelForDivider(R.id.divider_line_id)));
                    for (RecentlyClosedTab tab : tabs) {
                        LazyOneshotSupplier<Drawable> iconSupplier =
                                createIconSupplierForTab(
                                        tab.getUrl(),
                                        tab.getTabGroupId(),
                                        // Recently closed tabs are not tracked for incognito.
                                        /* isOffTheRecord= */ false,
                                        // No live Tab object is available to get a cached favicon.
                                        /* cachedFavicon= */ null,
                                        /* fallbackToHost= */ false);
                        submenuItems.add(
                                buildRecentEntryMenuItem(
                                        tab,
                                        TitleUtil.getTitleForDisplay(tab.getTitle(), tab.getUrl()),
                                        iconSupplier));
                    }
                    return submenuItems;
                };

        PropertyModel model =
                buildModelForMenuItemWithSubmenu(
                        R.id.recent_entry_menu_item,
                        getRecentEntrySubmenuTitle(group.getTitle(), group.getTabs().size()),
                        getTabGroupDrawable(group.getColor()),
                        submenuItemsSupplier);
        model.set(AppMenuItemProperties.ICON_NO_TINT, true);

        return new ListItem(AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU, model);
    }

    private ListItem buildRestoreGroupMenuItem(RecentlyClosedGroup group) {
        PropertyModel model =
                populateBaseModelForTextItem(
                                new PropertyModel.Builder(
                                        AppMenuRecentEntryItemProperties.ALL_KEYS),
                                R.id.recent_entry_group_menu_item)
                        .with(
                                AppMenuItemProperties.TITLE,
                                mContext.getString(R.string.menu_recent_entry_restore_group))
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, group)
                        .with(
                                AppMenuItemProperties.ICON,
                                AppCompatResources.getDrawable(
                                        mContext, R.drawable.ic_open_in_new_24dp))
                        .build();
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY, model);
    }

    private ListItem buildRecentEntryMenuItem(
            RecentlyClosedEntry entry,
            String title,
            @Nullable LazyOneshotSupplier<Drawable> iconSupplier) {
        PropertyModel.Builder builder =
                populateBaseModelForTextItem(
                                new PropertyModel.Builder(
                                        AppMenuRecentEntryItemProperties.ALL_KEYS),
                                R.id.recent_entry_tab_menu_item)
                        .with(AppMenuItemProperties.TITLE, title)
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, entry);
        if (shouldShowIconBeforeItem() && iconSupplier != null) {
            builder.with(AppMenuItemProperties.ICON_SUPPLIER, iconSupplier);
            builder.with(AppMenuItemProperties.ICON_NO_TINT, true);
        }
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY, builder.build());
    }

    private ListItem buildHistoryItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.open_history_menu_id,
                        R.string.menu_history,
                        shouldShowIconBeforeItem() ? R.drawable.ic_history_24dp : 0));
    }

    private ListItem buildDownloadsItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.downloads_menu_id,
                        R.string.menu_downloads,
                        shouldShowIconBeforeItem() ? R.drawable.ic_download_done_24dp : 0));
    }

    private boolean shouldShowBookmarksParentItem() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);
    }

    private ListItem buildBookmarksParentItem() {
        assert shouldShowBookmarksParentItem();

        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();

                    submenuItems.add(buildBookmarksItem());
                    submenuItems.add(buildBookmarkThisPageItem());
                    submenuItems.add(buildToggleBookmarksBarItem());

                    submenuItems.add(
                            new ListItem(
                                    AppMenuHandler.AppMenuItemType.DIVIDER,
                                    buildModelForDivider(R.id.divider_line_id)));

                    submenuItems.add(buildReadingListItem());

                    BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
                    if (bookmarkModel != null && bookmarkModel.isBookmarkModelLoaded()) {
                        List<ListItem> bookmarksBarItems =
                                getBookmarkItemList(
                                        BookmarkUtils.getDesktopBookmarkIds(bookmarkModel),
                                        bookmarkModel);
                        if (bookmarksBarItems.size() > 0) {
                            submenuItems.add(
                                    new ListItem(
                                            AppMenuHandler.AppMenuItemType.DIVIDER,
                                            buildModelForDivider(R.id.divider_line_id)));
                            submenuItems.addAll(bookmarksBarItems);
                        }

                        submenuItems.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.DIVIDER,
                                        buildModelForDivider(R.id.divider_line_id)));

                        submenuItems.add(
                                buildBookmarkFolderParentItem(
                                        R.string.menu_mobile_bookmarks,
                                        Arrays.asList(
                                                bookmarkModel.getAccountMobileFolderId(),
                                                bookmarkModel.getMobileFolderId())));

                        submenuItems.add(
                                buildBookmarkFolderParentItem(
                                        R.string.menu_other_bookmarks,
                                        Arrays.asList(
                                                bookmarkModel.getAccountOtherFolderId(),
                                                bookmarkModel.getOtherFolderId())));
                    }

                    return submenuItems;
                };

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.bookmarks_parent_menu_id,
                        R.string.menu_bookmarks,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_star_filled_24dp
                                : Resources.ID_NULL,
                        submenuItemsSupplier));
    }

    private ListItem buildBookmarkFolderParentItem(
            @StringRes int titleRes, List<BookmarkId> folderIds) {
        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> items = new ArrayList<>();
                    BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
                    if (bookmarkModel != null && bookmarkModel.isBookmarkModelLoaded()) {
                        List<BookmarkId> childIds = new ArrayList<>();
                        for (BookmarkId folderId : folderIds) {
                            if (folderId != null) {
                                childIds.addAll(bookmarkModel.getChildIds(folderId));
                            }
                        }
                        items.addAll(getBookmarkItemList(childIds, bookmarkModel));
                    }
                    if (items.size() == 0) {
                        items.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.EMPTY,
                                        new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                                                .with(AppMenuItemProperties.ENABLED, false)
                                                .build()));
                    }
                    return items;
                };

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.bookmark_folder_menu_id,
                        titleRes,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_folder_outline_24dp
                                : Resources.ID_NULL,
                        submenuItemsSupplier));
    }

    private ListItem buildReadingListItem() {
        List<ListItem> submenuItems = new ArrayList<>();

        submenuItems.add(
                new ListItem(
                        AppMenuHandler.AppMenuItemType.STANDARD,
                        buildModelForStandardMenuItem(
                                R.id.add_to_reading_list_menu_id,
                                R.string.menu_add_to_reading_list,
                                shouldShowIconBeforeItem() ? R.drawable.ic_list_add_24dp : 0)));

        submenuItems.add(
                new ListItem(
                        AppMenuHandler.AppMenuItemType.STANDARD,
                        buildModelForStandardMenuItem(
                                R.id.show_reading_list_menu_id,
                                R.string.menu_show_reading_list,
                                shouldShowIconBeforeItem() ? R.drawable.ic_list_24dp : 0)));

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.reading_list_parent_menu_id,
                        R.string.menu_reading_list,
                        shouldShowIconBeforeItem() ? R.drawable.ic_list_24dp : Resources.ID_NULL,
                        () -> submenuItems));
    }

    private ListItem buildToggleBookmarksBarItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.toggle_bookmarks_bar_menu_id,
                        BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(
                                        mTabModelSelector.getCurrentModel().getProfile())
                                ? R.string.menu_hide_bookmarks_bar
                                : R.string.menu_show_bookmarks_bar,
                        shouldShowIconBeforeItem() ? R.drawable.ic_toolbar_24dp : 0));
    }

    private BookmarkImageFetcher getImageFetcher() {
        if (mImageFetcher == null) {
            Profile profile = getProfileFromTabModel();
            BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
            assert bookmarkModel != null;
            mImageFetcher =
                    new BookmarkImageFetcher(
                            profile,
                            mContext,
                            bookmarkModel,
                            ImageFetcherFactory.createImageFetcher(
                                    ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                    profile.getProfileKey(),
                                    GlobalDiscardableReferencePool.getReferencePool()),
                            FaviconUtils.createCircularIconGenerator(mContext));
        }
        return mImageFetcher;
    }

    private List<ListItem> getBookmarkItemList(List<BookmarkId> ids, BookmarkModel bookmarkModel) {
        List<ListItem> submenuItems = new ArrayList<>();
        for (BookmarkId id : ids) {
            BookmarkItem item = bookmarkModel.getBookmarkById(id);
            if (item != null) {
                submenuItems.add(buildBookmarkListItem(item, bookmarkModel));
            }
        }
        return submenuItems;
    }

    private ListItem buildBookmarkListItem(BookmarkItem item, BookmarkModel bookmarkModel) {
        if (item.isFolder()) {
            return new ListItem(
                    AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                    buildModelForMenuItemWithSubmenu(
                            R.id.bookmark_folder_menu_id,
                            item.getTitle(),
                            shouldShowIconBeforeItem()
                                    ? R.drawable.ic_folder_outline_24dp
                                    : Resources.ID_NULL,
                            () -> {
                                List<ListItem> items =
                                        getBookmarkItemList(
                                                bookmarkModel.getChildIds(item.getId()),
                                                bookmarkModel);
                                if (items.size() == 0) {
                                    items.add(
                                            new ListItem(
                                                    AppMenuHandler.AppMenuItemType.EMPTY,
                                                    new PropertyModel.Builder(
                                                                    AppMenuItemProperties.ALL_KEYS)
                                                            .with(
                                                                    AppMenuItemProperties.ENABLED,
                                                                    false)
                                                            .build()));
                                }
                                return items;
                            }));
        } else {
            PropertyModel model =
                    populateBaseModelForTextItem(
                                    new PropertyModel.Builder(
                                            AppMenuBookmarkItemProperties.ALL_KEYS),
                                    R.id.bookmark_menu_id)
                            .with(AppMenuItemProperties.TITLE, item.getTitle())
                            .with(AppMenuBookmarkItemProperties.BOOKMARK_ID, item.getId())
                            .with(
                                    AppMenuItemProperties.ICON_SUPPLIER,
                                    shouldShowIconBeforeItem()
                                            ? createIconSupplierForBookmark(item)
                                            : null)
                            .with(AppMenuItemProperties.ICON_NO_TINT, !item.isFolder())
                            .build();
            return new ListItem(AppMenuHandler.AppMenuItemType.BOOKMARK, model);
        }
    }

    private LazyOneshotSupplier<Drawable> createIconSupplierForBookmark(BookmarkItem item) {
        if (item.isFolder()) {
            return LazyOneshotSupplier.fromSupplier(
                    () ->
                            AppCompatResources.getDrawable(
                                    mContext, R.drawable.ic_folder_outline_24dp));
        }
        return new LazyOneshotSupplierImpl<>() {
            @Override
            public void doSet() {
                getImageFetcher().fetchFaviconForBookmark(item, this::set);
            }
        };
    }

    private ListItem buildBookmarksItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.all_bookmarks_menu_id,
                        R.string.menu_bookmarks,
                        shouldShowIconBeforeItem() ? R.drawable.ic_star_filled_24dp : 0));
    }

    private ListItem buildBookmarkThisPageItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.bookmark_this_page_menu_id,
                        R.string.menu_bookmark_this_page,
                        shouldShowIconBeforeItem() ? R.drawable.ic_star_filled_24dp : 0));
    }

    private boolean shouldShowRecentTabsItem() {
        return !isIncognitoShowing();
    }

    private ListItem buildRecentTabsItem() {
        assert shouldShowRecentTabsItem();
        return new ListItem(
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

    private ListItem buildExtensionsParentItem() {
        assert shouldShowExtensionsItem();

        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildExtensionsMenuItem());
        submenuItems.add(buildManageExtensionsItem());
        submenuItems.add(buildChromeWebstoreItem());

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.extensions_parent_menu_id,
                        R.string.menu_extensions,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_extension_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems));
    }

    private ListItem buildExtensionsMenuItem() {
        assert shouldShowExtensionsItem();

        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.extensions_menu_menu_id,
                        R.string.menu_extensions_menu,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_extension_24dp
                                : Resources.ID_NULL));
    }

    private ListItem buildManageExtensionsItem() {
        assert shouldShowExtensionsItem();

        // The id {@code R.id.extensions_menu_id} is used for both when this flag is enabled and
        // disabled but in different context.
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);

        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.manage_extensions_menu_id,
                        R.string.menu_manage_extensions,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_extension_24dp
                                : Resources.ID_NULL));
    }

    private ListItem buildChromeWebstoreItem() {
        assert shouldShowExtensionsItem();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.extensions_webstore_menu_id,
                        R.string.menu_chrome_webstore,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_webstore_menu
                                : Resources.ID_NULL));
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

        if (shouldShowPaintPreview(isNativePage, currentTab)) {
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

        if (shouldShowDownloadPageMenuItem(currentTab)) {
            submenuItems.add(buildDownloadPageItem(currentTab));
        }

        if (shouldShowHomeScreenMenuItem(
                isNativePage, isFileScheme, isContentScheme, isIncognitoShowing(), url)) {
            assert currentTab != null;
            submenuItems.add(buildAddToHomescreenListItem(currentTab, shouldShowIconBeforeItem()));
        }

        if (shouldShowPaintPreview(isNativePage, currentTab)) {
            submenuItems.add(buildPaintPreviewItem(isNativePage, currentTab));
        }

        if (ShareUtils.shouldEnableShare(currentTab)) {
            if (!submenuItems.isEmpty()) {
                submenuItems.add(
                        new ListItem(
                                AppMenuHandler.AppMenuItemType.DIVIDER,
                                buildModelForDivider(R.id.divider_line_id)));
            }
            submenuItems.add(buildShareListItem(shouldShowIconBeforeItem()));
            submenuItems.add(buildCopyLinkItem());
            submenuItems.add(buildSendToDevicesItem());
            submenuItems.add(buildShareQrCodeItem());
        }

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.save_and_share_parent_menu_id,
                        R.string.menu_save_and_share,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_file_save_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems));
    }

    private ListItem buildCopyLinkItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.copy_link_menu_id,
                        R.string.menu_copy_link,
                        shouldShowIconBeforeItem() ? R.drawable.ic_copy_link_24dp : 0));
    }

    private ListItem buildSendToDevicesItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.send_to_devices_menu_id,
                        R.string.menu_send_to_devices,
                        shouldShowIconBeforeItem() ? R.drawable.send_tab : 0));
    }

    private ListItem buildShareQrCodeItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.qr_code_menu_id,
                        R.string.menu_qr_code,
                        shouldShowIconBeforeItem() ? R.drawable.qr_code : 0));
    }

    private ListItem buildDownloadPageItem(Tab currentTab) {
        assert shouldShowDownloadPageMenuItem(currentTab);
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.download_page_id,
                        R.string.menu_download_page,
                        shouldShowIconBeforeItem() ? R.drawable.ic_file_download_white_24dp : 0));
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
                buildModelForStandardMenuItem(
                        R.id.print_id,
                        R.string.menu_print,
                        shouldShowIconBeforeItem() ? R.drawable.sharing_print : 0));
    }

    private boolean shouldShowTaskManagerItem() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TASK_MANAGER_CLANK);
    }

    private ListItem buildTaskManagerItem() {
        assert shouldShowTaskManagerItem();

        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.task_manager,
                        R.string.menu_task_manager,
                        shouldShowIconBeforeItem() ? R.drawable.ic_task_manager_24dp : 0));
    }

    private boolean shouldShowDevToolsItem(@Nullable Tab currentTab) {
        if (!ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)) {
            return false;
        }

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            return false;
        }

        if (currentTab == null || currentTab.isNativePage()) {
            return false;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return false;
        }

        return DevToolsWindowAndroid.isDevToolsAllowedFor(currentTab.getProfile(), webContents);
    }

    private ListItem buildDevToolsItem(@Nullable Tab currentTab) {
        assert shouldShowDevToolsItem(currentTab);

        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.dev_tools,
                        R.string.menu_dev_tools,
                        shouldShowIconBeforeItem() ? R.drawable.ic_dev_tools_24dp : 0));
    }

    private boolean shouldShowMoreToolsItem(@Nullable Tab currentTab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            return false;
        }

        if (shouldShowReaderModeItem(currentTab)) {
            return true;
        }

        if (shouldShowTaskManagerItem()) {
            return true;
        }

        if (shouldShowDevToolsItem(currentTab)) {
            return true;
        }

        if (shouldShowNameWindowItem()) {
            return true;
        }

        if (shouldShowTabLayoutToggleItem()) {
            return true;
        }

        return false;
    }

    private ListItem buildMoreToolsItem(@Nullable Tab currentTab) {
        assert shouldShowMoreToolsItem(currentTab);

        List<ListItem> submenuItems = new ArrayList<>();
        if (shouldShowReaderModeItem(currentTab)) {
            submenuItems.add(buildReaderModeItem(currentTab));
        }

        if (shouldShowTaskManagerItem()) {
            submenuItems.add(buildTaskManagerItem());
        }

        if (shouldShowDevToolsItem(currentTab)) {
            submenuItems.add(buildDevToolsItem(currentTab));
        }

        if (shouldShowNameWindowItem()) {
            submenuItems.add(buildNameWindowItem());
        }

        if (shouldShowTabLayoutToggleItem()) {
            submenuItems.add(buildTabLayoutToggleItem());
        }

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.more_tools_menu_id,
                        R.string.menu_more_tools,
                        shouldShowIconBeforeItem()
                                ? R.drawable.ic_more_tools_24dp
                                : Resources.ID_NULL,
                        () -> submenuItems));
    }

    private boolean shouldShowNameWindowItem() {
        return MultiWindowUtils.isMultiInstanceApi31Enabled();
    }

    private ListItem buildNameWindowItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.name_window_menu_id,
                        R.string.menu_name_window,
                        shouldShowIconBeforeItem() ? R.drawable.ic_window_24dp : 0));
    }

    private boolean shouldShowTabLayoutToggleItem() {
        return VerticalTabUtils.isVerticalTabsEligible(mContext);
    }

    private ListItem buildTabLayoutToggleItem() {
        int stringRes =
                VerticalTabUtils.isVerticalTabsEnabled(mContext)
                        ? org.chromium.chrome.tab_ui.R.string.show_tabs_horizontally
                        : org.chromium.chrome.tab_ui.R.string.show_tabs_vertically;
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.toggle_tab_layout_menu_id,
                        stringRes,
                        shouldShowIconBeforeItem() ? R.drawable.ic_dock_to_right_24dp : 0));
    }

    @Contract("null -> false")
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
                buildModelForStandardMenuItem(
                        R.id.get_image_descriptions_id,
                        titleId,
                        shouldShowIconBeforeItem() ? R.drawable.ic_image_descriptions : 0));
    }

    private ListItem buildNewTabGroupItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.new_tab_group_menu_id,
                        R.string.menu_new_tab_group,
                        shouldShowIconBeforeItem() ? R.drawable.ic_widgets : 0));
    }

    private ListItem buildCloseAllTabsItem() {
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
                buildModelForStandardMenuItem(
                        R.id.menu_select_tabs,
                        R.string.menu_select_tabs,
                        shouldShowIconBeforeItem() ? R.drawable.ic_select_check_box_24dp : 0);
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
                buildModelForStandardMenuItem(
                        R.id.preferences_id,
                        R.string.menu_settings,
                        shouldShowIconBeforeItem() ? R.drawable.settings_cog : 0));
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
    @Contract("null -> false")
    private boolean shouldShowNtpCustomizations(@Nullable Tab currentTab) {
        return !isIncognitoShowing()
                && currentTab != null
                && UrlUtilities.isNtpUrl(currentTab.getUrl());
    }

    private ListItem buildNtpCustomizationsItem(Tab currentTab) {
        assert shouldShowNtpCustomizations(currentTab);
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.ntp_customization_id,
                        R.string.menu_ntp_customization,
                        shouldShowIconBeforeItem() ? R.drawable.ic_edit_24dp : 0));
    }

    private ListItem buildHelpItem() {
        int helpString = HelpAndFeedbackLauncher.getHelpMenuStringRes();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.help_id,
                        helpString,
                        shouldShowIconBeforeItem() ? R.drawable.ic_help_24dp : 0));
    }

    private ListItem buildHelpParentItem() {
        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(buildAboutChromeItem());
        submenuItems.add(buildHelpCenterItem());
        submenuItems.add(buildReportIssueItem());

        int helpString = HelpAndFeedbackLauncher.getHelpMenuStringRes();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                buildModelForMenuItemWithSubmenu(
                        R.id.help_parent_menu_id,
                        helpString,
                        shouldShowIconBeforeItem() ? R.drawable.ic_help_24dp : Resources.ID_NULL,
                        () -> submenuItems));
    }

    private ListItem buildHelpCenterItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.help_id,
                        R.string.menu_help_center,
                        shouldShowIconBeforeItem() ? R.drawable.ic_help_24dp : 0));
    }

    private ListItem buildReportIssueItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.report_issue_menu_id,
                        R.string.menu_report_issue,
                        shouldShowIconBeforeItem() ? R.drawable.ic_feedback_24dp : 0));
    }

    private ListItem buildAboutChromeItem() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.about_chrome_menu_id,
                        R.string.menu_about_chrome,
                        shouldShowIconBeforeItem() ? R.drawable.ic_info_24dp : 0));
    }

    private boolean shouldShowQuickDeleteItem() {
        return !isIncognitoShowing();
    }

    private ListItem buildQuickDeleteItem() {
        assert shouldShowQuickDeleteItem();
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.quick_delete_menu_id,
                        R.string.menu_quick_delete,
                        shouldShowIconBeforeItem() ? R.drawable.material_ic_delete_24dp : 0));
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
                buildModelForStandardMenuItem(
                        R.id.find_in_page_id,
                        R.string.menu_find_in_page,
                        shouldShowIconBeforeItem() ? R.drawable.ic_find_in_page : 0));
    }

    private boolean shouldShowLensOverlayItem(@Nullable Tab currentTab) {
        return LensOverlayTabHelper.shouldShowLensOverlay(currentTab);
    }

    private MVCListAdapter.ListItem buildLensOverlayItem(@Nullable Tab currentTab) {
        assert shouldShowLensOverlayItem(currentTab);
        PropertyModel model =
                buildModelForStandardMenuItem(
                        R.id.lens_overlay_menu_id,
                        R.string.menu_search_tab_with_google_lens,
                        shouldShowIconBeforeItem()
                                ? R.drawable.lens_camera_icon
                                : Resources.ID_NULL);

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
                buildModelForStandardMenuItem(
                        R.id.default_browser_promo_menu_id, R.string.make_chrome_default, 0);

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
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.glic_menu_id,
                        R.string.glic_button_entrypoint_open_gemini_label,
                        shouldShowIconBeforeItem() ? R.drawable.ic_spark_24dp : Resources.ID_NULL));
    }

    /**
     * @param isNativePage Whether the current tab is a native page.
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the paint preview menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @Contract("_, null -> false")
    public boolean shouldShowPaintPreview(boolean isNativePage, @Nullable Tab currentTab) {
        return currentTab != null
                && ChromeFeatureList.sPaintPreviewDemo.isEnabled()
                && !isNativePage
                && !isIncognitoShowing();
    }

    private ListItem buildPaintPreviewItem(boolean isNativePage, Tab currentTab) {
        assert shouldShowPaintPreview(isNativePage, currentTab);
        return new ListItem(
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
                buildModelForStandardMenuItem(
                        R.id.managed_by_menu_id,
                        R.string.managed_browser,
                        shouldShowIconBeforeItem() ? R.drawable.ic_domain : 0));
    }

    private ListItem buildContentFilterHelpCenterMenuItem(Tab currentTab) {
        assert shouldShowContentFilterHelpCenterMenuItem(currentTab);
        return new ListItem(
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

    private Profile getProfileFromTabModel() {
        var profile = mTabModelSelector.getModel(false).getProfile();
        assert profile != null;
        return profile;
    }

    public void setImageFetcherForTesting(BookmarkImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }
}
