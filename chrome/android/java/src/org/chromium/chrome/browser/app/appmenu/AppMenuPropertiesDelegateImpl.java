// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.PopupMenu;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.common.primitives.UnsignedLongs;

import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler.AppMenuItemType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.appmenu.AppMenuUtil;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Iterator;
import java.util.List;

/**
 * Base implementation of {@link AppMenuPropertiesDelegate} that handles hiding and showing menu
 * items based on activity state.
 */
public abstract class AppMenuPropertiesDelegateImpl implements AppMenuPropertiesDelegate {
    private static Boolean sItemBookmarkedForTesting;

    protected final Context mContext;
    protected final boolean mIsTablet;
    protected final ActivityTabProvider mActivityTabProvider;
    protected final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    protected final TabModelSelector mTabModelSelector;
    protected final ToolbarManager mToolbarManager;
    protected final View mDecorView;

    private CallbackController mCallbackController = new CallbackController();
    private ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private final Supplier<ReadAloudController> mReadAloudControllerSupplier;
    private @Nullable ModelList mModelList;
    private int mReadAloudPos;
    @Nullable protected Runnable mReadAloudAppMenuResetter;
    private boolean mHasReadAloudInserted;

    @VisibleForTesting
    @IntDef({
        MenuGroup.INVALID,
        MenuGroup.PAGE_MENU,
        MenuGroup.OVERVIEW_MODE_MENU,
        MenuGroup.TABLET_EMPTY_MODE_MENU
    })
    public @interface MenuGroup {
        int INVALID = -1;
        int PAGE_MENU = 0;
        int OVERVIEW_MODE_MENU = 1;
        int TABLET_EMPTY_MODE_MENU = 2;
    }

    // Please treat this list as append only and keep it in sync with
    // AppMenuHighlightItem in enums.xml.
    @IntDef({
        AppMenuHighlightItem.UNKNOWN,
        AppMenuHighlightItem.DOWNLOADS,
        AppMenuHighlightItem.BOOKMARKS,
        AppMenuHighlightItem.TRANSLATE,
        AppMenuHighlightItem.ADD_TO_HOMESCREEN,
        AppMenuHighlightItem.DOWNLOAD_THIS_PAGE,
        AppMenuHighlightItem.BOOKMARK_THIS_PAGE,
        AppMenuHighlightItem.DATA_REDUCTION_FOOTER
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AppMenuHighlightItem {
        int UNKNOWN = 0;
        int DOWNLOADS = 1;
        int BOOKMARKS = 2;
        int TRANSLATE = 3;
        int ADD_TO_HOMESCREEN = 4;
        int DOWNLOAD_THIS_PAGE = 5;
        int BOOKMARK_THIS_PAGE = 6;
        int DATA_REDUCTION_FOOTER = 7;
        int NUM_ENTRIES = 8;
    }

    private @Nullable LayoutStateProvider mLayoutStateProvider;
    protected Runnable mAppMenuInvalidator;

    /**
     * Construct a new {@link AppMenuPropertiesDelegateImpl}.
     *
     * @param context The activity context.
     * @param activityTabProvider The {@link ActivityTabProvider} for the containing activity.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} for the
     *     containing activity.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param toolbarManager The {@link ToolbarManager} for the containing activity.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *     activity.
     * @param layoutStateProvidersSupplier An {@link ObservableSupplier} for the {@link
     *     LayoutStateProvider} associated with the containing activity.
     * @param bookmarkModelSupplier An {@link ObservableSupplier} for the {@link BookmarkModel}
     */
    protected AppMenuPropertiesDelegateImpl(
            Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager,
            View decorView,
            @Nullable OneshotSupplier<LayoutStateProvider> layoutStateProvidersSupplier,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            @Nullable Supplier<ReadAloudController> readAloudControllerSupplier) {
        mContext = context;
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        mActivityTabProvider = activityTabProvider;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mTabModelSelector = tabModelSelector;
        mToolbarManager = toolbarManager;
        mDecorView = decorView;
        mReadAloudControllerSupplier = readAloudControllerSupplier;

        if (layoutStateProvidersSupplier != null) {
            layoutStateProvidersSupplier.onAvailable(
                    mCallbackController.makeCancelable(
                            layoutStateProvider -> {
                                mLayoutStateProvider = layoutStateProvider;
                            }));
        }

        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    @Override
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        if (mReadAloudControllerSupplier.get() != null) {
            mReadAloudControllerSupplier
                    .get()
                    .removeReadabilityUpdateListener(mReadAloudAppMenuResetter);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public Runnable getReadAloudmenuResetter() {
        return mReadAloudAppMenuResetter;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @Nullable
    public ModelList getModelList() {
        return mModelList;
    }

    /**
     * @return The resource id for the menu to use in {@link AppMenu}.
     */
    protected int getAppMenuLayoutId() {
        return R.menu.main_menu;
    }

    /**
     * @return Whether the app menu for a web page should be shown.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowPageMenu() {
        boolean isInTabSwitcher = isInTabSwitcher();
        if (mIsTablet) {
            boolean hasTabs = mTabModelSelector.getCurrentModel().getCount() != 0;
            return hasTabs && !isInTabSwitcher;
        } else {
            return !isInTabSwitcher;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @MenuGroup
    public int getMenuGroup() {
        // Determine which menu to show.
        @MenuGroup int menuGroup = MenuGroup.INVALID;
        if (shouldShowPageMenu()) menuGroup = MenuGroup.PAGE_MENU;

        boolean isInTabSwitcher = isInTabSwitcher();
        if (mIsTablet) {
            boolean hasTabs = mTabModelSelector.getCurrentModel().getCount() != 0;
            if (hasTabs && isInTabSwitcher) {
                menuGroup = MenuGroup.OVERVIEW_MODE_MENU;
            } else if (!hasTabs) {
                menuGroup = MenuGroup.TABLET_EMPTY_MODE_MENU;
            }
        } else if (isInTabSwitcher) {
            menuGroup = MenuGroup.OVERVIEW_MODE_MENU;
        }
        assert menuGroup != MenuGroup.INVALID;
        return menuGroup;
    }

    /**
     * @return Whether the grid tab switcher is showing.
     */
    private boolean isInTabSwitcher() {
        return mLayoutStateProvider != null
                && mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)
                && !mLayoutStateProvider.isLayoutStartingToHide(LayoutType.TAB_SWITCHER);
    }

    @Override
    public ModelList getMenuItems(AppMenuHandler handler) {
        mReadAloudPos = -1;
        PopupMenu popup = new PopupMenu(mContext, mDecorView);
        Menu menu = popup.getMenu();
        MenuInflater inflater = popup.getMenuInflater();
        inflater.inflate(getAppMenuLayoutId(), menu);

        mModelList = getMenuItemsForMenu(menu, handler);
        return mModelList;
    }

    /**
     * Builds a property model for a divider item type.
     *
     * @param id The id of the divider.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForDivider(@IdRes int id) {
        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .with(AppMenuItemProperties.SUPPORT_ENTER_ANIMATION, true)
                .build();
    }

    /**
     * Constructs the basis for text menu items models.
     *
     * @param id The id of the text menu item.
     * @return A Builder object that forms the basis for text menu item models.
     */
    public PropertyModel.Builder buildBaseModelForTextItem(@IdRes int id) {
        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .with(AppMenuItemProperties.ENABLED, true)
                .with(AppMenuItemProperties.ICON_COLOR_RES, getMenuItemIconColorRes(id))
                .with(AppMenuItemProperties.ICON_SHOW_BADGE, shouldShowBadgeOnMenuItemIcon(id))
                .with(AppMenuItemProperties.SUPPORT_ENTER_ANIMATION, true)
                .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart())
                .with(AppMenuItemProperties.TITLE_CONDENSED, getContentDescription(id))
                .with(AppMenuItemProperties.MANAGED, isMenuItemManaged(id));
    }

    /**
     * Build a property model for a standard text menu item.
     *
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @return The property model for this item.
     */
    public PropertyModel buildModelForStandardMenuItem(
            @IdRes int id, @StringRes int titleId, @DrawableRes int iconResId) {
        PropertyModel model =
                buildBaseModelForTextItem(id)
                        .with(AppMenuItemProperties.TITLE, mContext.getString(titleId))
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for a text menu item w/ checkbox.
     *
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param checkBoxId The id of the checkbox item.
     * @param isChecked Whether the checkbox is currently checked.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForMenuItemWithCheckbox(
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            @IdRes int checkBoxId,
            boolean isChecked) {
        PropertyModel checkBoxModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_ICON_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, checkBoxId)
                        .with(AppMenuItemProperties.CHECKABLE, true)
                        .with(AppMenuItemProperties.CHECKED, isChecked)
                        .with(AppMenuItemProperties.ENABLED, true)
                        .build();
        ModelList subList = new ModelList();
        subList.add(new ListItem(0, checkBoxModel));

        PropertyModel model =
                buildBaseModelForTextItem(id)
                        .with(AppMenuItemProperties.TITLE, mContext.getString(titleId))
                        .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for a text menu item w/ secondary action button.
     *
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param secondaryActionId The id of the secondary action.
     * @param secondaryActionTitle The title for the secondary action.
     * @param secondaryActionIcon The icon for the secondary action.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForMenuItemWithSecondaryButton(
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            @IdRes int secondaryActionId,
            CharSequence secondaryActionTitle,
            Drawable secondaryActionIcon) {
        PropertyModel secondaryActionModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, secondaryActionId)
                        .with(AppMenuItemProperties.TITLE, secondaryActionTitle)
                        .with(AppMenuItemProperties.ICON, secondaryActionIcon)
                        .with(AppMenuItemProperties.ENABLED, true)
                        .build();

        ModelList subList = new ModelList();
        subList.add(new MVCListAdapter.ListItem(0, secondaryActionModel));

        PropertyModel model =
                buildBaseModelForTextItem(id)
                        .with(AppMenuItemProperties.TITLE, mContext.getString(titleId))
                        .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for an icon row button.
     *
     * @param id The id of the menu item.
     * @param titleId The resource id of the title for this icon.
     * @param titleCondensedId The resource id of the condensed title for this icon, which is used
     *     for accessibility.
     * @param iconResId The resource id of the icon to be displayed.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForIcon(
            @IdRes int id,
            @StringRes int titleId,
            @StringRes int titleCondensedId,
            @DrawableRes int iconResId) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_ICON_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                        .with(AppMenuItemProperties.TITLE, mContext.getString(titleId))
                        .with(
                                AppMenuItemProperties.TITLE_CONDENSED,
                                mContext.getString(titleCondensedId))
                        .with(AppMenuItemProperties.ENABLED, true)
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for an icon row.
     *
     * @param id The id of the menu item.
     * @param iconModels The list of models representing the icons in the row.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForIconRow(@IdRes int id, List<PropertyModel> iconModels) {
        ModelList subList = new ModelList();
        for (PropertyModel iconModel : iconModels) {
            subList.add(new MVCListAdapter.ListItem(0, iconModel));
        }

        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart())
                .build();
    }

    /**
     * Return the item type for the given menu item ID, and the default value is {@link
     * AppMenuItemType#STANDARD}.
     */
    protected int getItemTypeForId(@IdRes int menuItemId) {
        return AppMenuItemType.STANDARD;
    }

    /** Build and return the appropriate type/model ListItem for the given MenuItem. */
    // TODO(crbug.com/40145539): Delete this method after inflation from XML is removed.
    protected MVCListAdapter.ListItem buildPropertyModelForMenuItem(MenuItem item) {
        PropertyModel propertyModel;
        @AppMenuItemType int menuType;
        if (item.getItemId() == R.id.icon_row_menu_id) {
            menuType = AppMenuItemType.BUTTON_ROW;

            ModelList subList = new ModelList();
            assert item.getSubMenu().size() <= 5 : "At most 5 buttons currently supported.";
            for (int j = 0; j < item.getSubMenu().size(); j++) {
                MenuItem subitem = item.getSubMenu().getItem(j);
                if (!subitem.isVisible()) continue;

                PropertyModel subModel = AppMenuUtil.buildPropertyModelForIcon(subitem);
                subList.add(new MVCListAdapter.ListItem(0, subModel));
                if (subitem.getItemId() == R.id.reload_menu_id) {
                    Tab currentTab = mActivityTabProvider.get();
                    updateReloadPropertyModel(
                            subModel, currentTab != null && currentTab.isLoading());
                }
            }
            propertyModel =
                    new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                            .with(AppMenuItemProperties.MENU_ITEM_ID, item.getItemId())
                            .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                            .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart())
                            .build();
        } else if (item.getItemId() == R.id.divider_line_id
                || item.getItemId() == R.id.managed_by_divider_line_id
                || item.getItemId() == R.id.quick_delete_divider_line_id) {
            menuType = AppMenuItemType.DIVIDER;
            propertyModel =
                    new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                            .with(AppMenuItemProperties.MENU_ITEM_ID, item.getItemId())
                            .with(AppMenuItemProperties.SUPPORT_ENTER_ANIMATION, true)
                            .build();
        } else {
            boolean isTitleButtonType = item.hasSubMenu() && item.getSubMenu().size() == 2;
            MenuItem textItem = item;
            if (isTitleButtonType) {
                menuType = AppMenuItemType.TITLE_BUTTON;
                textItem = item.getSubMenu().getItem(0);
            } else {
                menuType = getItemTypeForId(item.getItemId());
            }

            propertyModel = AppMenuUtil.menuItemToPropertyModel(textItem);
            propertyModel.set(
                    AppMenuItemProperties.ICON_COLOR_RES,
                    getMenuItemIconColorRes(textItem.getItemId()));
            propertyModel.set(
                    AppMenuItemProperties.ICON_SHOW_BADGE,
                    shouldShowBadgeOnMenuItemIcon(textItem.getItemId()));
            propertyModel.set(AppMenuItemProperties.SUPPORT_ENTER_ANIMATION, true);
            propertyModel.set(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart());
            propertyModel.set(
                    AppMenuItemProperties.TITLE_CONDENSED,
                    getContentDescription(textItem.getItemId()));
            propertyModel.set(
                    AppMenuItemProperties.MANAGED, isMenuItemManaged(textItem.getItemId()));

            if (isTitleButtonType) {
                ModelList subList = new ModelList();
                MenuItem subitem = item.getSubMenu().getItem(1);
                assert subitem.isVisible();
                PropertyModel subModel = AppMenuUtil.buildPropertyModelForIcon(subitem);
                subList.add(new MVCListAdapter.ListItem(0, subModel));
                propertyModel.set(AppMenuItemProperties.ADDITIONAL_ICONS, subList);
            }
        }
        return new MVCListAdapter.ListItem(menuType, propertyModel);
    }

    // TODO(crbug.com/40145539): Delete this method after inflation from XML is removed.
    @VisibleForTesting
    public ModelList getMenuItemsForMenu(Menu menu, AppMenuHandler handler) {
        ModelList modelList = new ModelList();
        prepareMenu(menu, handler);
        // TODO(crbug.com/40145539): Programmatically create menu item's PropertyModel instead of
        // converting from MenuItems.
        int visibleBeforeReadAloudCount = 0;
        for (int i = 0; i < menu.size(); ++i) {
            MenuItem item = menu.getItem(i);
            if (!item.isVisible()) {
                if (item.getItemId() == R.id.readaloud_menu_id) {
                    mReadAloudPos = visibleBeforeReadAloudCount;
                }
                continue;
            }
            visibleBeforeReadAloudCount++;

            modelList.add(buildPropertyModelForMenuItem(item));
        }
        int lastIndex = modelList.size() - 1;
        if (modelList.get(lastIndex).type == AppMenuItemType.DIVIDER) {
            modelList.removeAt(lastIndex);
        }
        return modelList;
    }

    @Override
    public abstract void prepareMenu(Menu menu, AppMenuHandler handler);

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the reader mode preferences menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowReaderModePrefs(@NonNull Tab currentTab) {
        return DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl());
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
        if (sItemBookmarkedForTesting != null) return sItemBookmarkedForTesting;

        if (!mBookmarkModelSupplier.hasValue()) return false;
        return mBookmarkModelSupplier.get().hasBookmarkIdForTab(currentTab);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public boolean instanceSwitcherWithMultiInstanceEnabled() {
        return MultiWindowUtils.instanceSwitcherEnabled()
                && MultiWindowUtils.isMultiInstanceApi31Enabled();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public boolean isTabletSizeScreen() {
        return mIsTablet;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public boolean isAutoDarkWebContentsEnabled() {
        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        assert profile != null;
        boolean isFlagEnabled =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING);
        boolean isFeatureEnabled =
                WebContentsDarkModeController.isFeatureEnabled(mContext, profile);
        return isFlagEnabled && isFeatureEnabled;
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the currentTab should show an app menu item that requires a webContents. This
     *     will return false for native NTP, and true otherwise.
     */
    protected boolean shouldShowWebContentsDependentMenuItem(@NonNull Tab currentTab) {
        return !currentTab.isNativePage() && currentTab.getWebContents() != null;
    }

    /**
     * This method should only be called once per context menu shown.
     *
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the translate menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowTranslateMenuItem(@NonNull Tab currentTab) {
        return TranslateUtils.canTranslateCurrentTab(currentTab, true);
    }

    /**
     * @param isNativePage Whether the current tab is a native page.
     * @param isFileScheme Whether URL for the current tab starts with the file:// scheme.
     * @param isContentScheme Whether URL for the current tab starts with the file:// scheme.
     * @param isIncognito Whether the current tab is incognito.
     * @param url The URL for the current tab.
     * @return Whether the homescreen menu item should be displayed.
     */
    protected boolean shouldShowHomeScreenMenuItem(
            boolean isNativePage,
            boolean isFileScheme,
            boolean isContentScheme,
            boolean isIncognito,
            @NonNull GURL url) {
        // Hide 'Add to homescreen' for the following:
        // * native pages - Android doesn't know how to direct those URLs.
        // * incognito pages - To avoid problems where users create shortcuts in incognito
        //                      mode and then open the webapp in regular mode.
        // * file:// - After API 24, file: URIs are not supported in VIEW intents and thus
        //             can not be added to the homescreen.
        // * content:// - Accessing external content URIs requires the calling app to grant
        //                access to the resource via FLAG_GRANT_READ_URI_PERMISSION, and that
        //                is not persisted when adding to the homescreen.
        // * If creating shortcuts it not supported by the current home screen.
        return WebappsUtils.isAddToHomeIntentSupported()
                && !isNativePage
                && !isFileScheme
                && !isContentScheme
                && !isIncognito
                && !url.isEmpty();
    }

    /**
     * @param currentTab Current tab being displayed.
     * @return Whether the "Managed by your organization" menu item should be displayed.
     */
    protected boolean shouldShowManagedByMenuItem(Tab currentTab) {
        return false;
    }

    /**
     * @param currentTab Current tab being displayed.
     * Returns whether the "Download page" menu item should be displayed.
     */
    protected boolean shouldShowDownloadPageMenuItem(Tab currentTab) {
        return ChromeFeatureList.sHideTabletToolbarDownloadButton.isEnabled()
                && isTabletSizeScreen()
                && shouldEnableDownloadPage(currentTab);
    }

    /** Sets the visibility and labels of the "Add to Home screen" and "Open WebAPK" menu items. */
    // TODO(crbug.com/40145539): Delete this method after inflation from XML is removed.
    protected void prepareAddToHomescreenMenuItem(
            Menu menu, Tab currentTab, boolean shouldShowHomeScreenMenuItem) {
        MenuItem universalInstallItem = menu.findItem(R.id.universal_install);
        MenuItem openWebApkItem = menu.findItem(R.id.open_webapk_id);

        universalInstallItem.setVisible(false);
        openWebApkItem.setVisible(false);

        if (currentTab == null || !shouldShowHomeScreenMenuItem) {
            return;
        }

        long addToHomeScreenStart = SystemClock.elapsedRealtime();
        ResolveInfo resolveInfo = queryWebApkResolveInfo(mContext, currentTab);
        RecordHistogram.recordTimesHistogram(
                "Android.PrepareMenu.OpenWebApkVisibilityCheck",
                SystemClock.elapsedRealtime() - addToHomeScreenStart);

        // When Universal Install is active, we only show this menu item if we are browsing
        // the root page of an already installed app.
        boolean openWebApkItemVisible =
                resolveInfo != null
                        && resolveInfo.activityInfo.packageName != null
                        && "/".equals(currentTab.getUrl().getPath());

        if (openWebApkItemVisible) {
            // This is the 'webapp is already installed' case, so we offer to open the webapp.
            String appName = resolveInfo.loadLabel(mContext.getPackageManager()).toString();
            openWebApkItem.setTitle(mContext.getString(R.string.menu_open_webapk, appName));
            openWebApkItem.setVisible(true);
            return;
        }

        universalInstallItem.setVisible(true);
    }

    /**
     * Builds the appropriate item for adding the current page to the homescreen of the device.
     *
     * @param currentTab The currently selected Tab.
     * @param showIcon Whether the icon should be shown for this item.
     * @return The add to homescreen list item.
     */
    protected ListItem buildAddToHomescreenListItem(Tab currentTab, boolean showIcon) {
        long addToHomeScreenStart = SystemClock.elapsedRealtime();
        ResolveInfo resolveInfo = queryWebApkResolveInfo(mContext, currentTab);
        RecordHistogram.recordTimesHistogram(
                "Android.PrepareMenu.OpenWebApkVisibilityCheck",
                SystemClock.elapsedRealtime() - addToHomeScreenStart);

        // When Universal Install is active, we only show this menu item if we are browsing
        // the root page of an already installed app.
        boolean openWebApkItemVisible =
                resolveInfo != null
                        && resolveInfo.activityInfo.packageName != null
                        && "/".equals(currentTab.getUrl().getPath());

        if (openWebApkItemVisible) {
            // This is the 'webapp is already installed' case, so we offer to open the webapp.
            String appName = resolveInfo.loadLabel(mContext.getPackageManager()).toString();
            return new ListItem(
                    AppMenuItemType.STANDARD,
                    buildBaseModelForTextItem(R.id.open_webapk_id)
                            .with(
                                    AppMenuItemProperties.TITLE,
                                    mContext.getString(R.string.menu_open_webapk, appName))
                            .with(
                                    AppMenuItemProperties.ICON,
                                    showIcon
                                            ? AppCompatResources.getDrawable(
                                                    mContext, R.drawable.ic_open_webapk)
                                            : null)
                            .build());
        } else {
            return new ListItem(
                    AppMenuItemType.STANDARD,
                    buildModelForStandardMenuItem(
                            R.id.universal_install,
                            R.string.menu_add_to_homescreen,
                            showIcon ? R.drawable.ic_add_to_home_screen : 0));
        }
    }

    public static ResolveInfo queryWebApkResolveInfo(Context context, Tab currentTab) {
        String manifestId = AppBannerManager.maybeGetManifestId(currentTab.getWebContents());
        ResolveInfo resolveInfo =
                WebApkValidator.queryFirstWebApkResolveInfo(
                        context,
                        currentTab.getUrl().getSpec(),
                        WebappRegistry.getInstance().findWebApkWithManifestId(manifestId));

        if (resolveInfo == null) {
            // If a WebAPK with matching manifestId can't be found, fallback to query without it.
            resolveInfo =
                    WebApkValidator.queryFirstWebApkResolveInfo(
                            context, currentTab.getUrl().getSpec());
        }

        return resolveInfo;
    }

    @Override
    public Bundle getBundleForMenuItem(int itemId) {
        return null;
    }

    /** Sets the visibility of the "Translate" menu item. */
    protected void prepareTranslateMenuItem(Menu menu, @Nullable Tab currentTab) {
        boolean isTranslateVisible = currentTab != null && shouldShowTranslateMenuItem(currentTab);
        menu.findItem(R.id.translate_id).setVisible(isTranslateVisible);
    }

    /** Sets visibility of the "Listen to this page" menu item. */
    protected void prepareReadAloudMenuItem(Menu menu, @Nullable Tab currentTab) {
        boolean visible = isTabReadable(currentTab);
        observeReadabilityUpdates(currentTab);
        mHasReadAloudInserted = visible;
        menu.findItem(R.id.readaloud_menu_id).setVisible(visible);
    }

    private void observeReadabilityUpdates(Tab currentTab) {
        ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
        if (readAloudController == null) return;

        if (mReadAloudAppMenuResetter == null) {
            mReadAloudAppMenuResetter =
                    () -> {
                        boolean isReadable = isTabReadable(currentTab);
                        if (isReadable) {
                            maybeInsertReadAloudItem(mModelList);
                        } else {
                            maybeFindAndRemoveReadAloudItem();
                        }
                    };
        }
        readAloudController.addReadabilityUpdateListener(mReadAloudAppMenuResetter);
    }

    private boolean isTabReadable(Tab tab) {
        ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
        return tab != null && readAloudController != null && readAloudController.isReadable(tab);
    }

    /**
     * Observes the state of readability for the current tab and adds the read aloud item if
     * appropriate.
     *
     * @param modelList The list where the read aloud option should be added if conditions allow.
     * @param currentTab The currently selected tab.
     */
    protected void observeAndMaybeAddReadAloud(ModelList modelList, Tab currentTab) {
        mReadAloudPos = modelList.size();
        observeReadabilityUpdates(currentTab);
        if (isTabReadable(currentTab)) {
            maybeInsertReadAloudItem(modelList);
        }
    }

    /**
     * Try finding ReadAloud in the mModelList (being in the model means it was visible in the app
     * menu). If found, remove it from the model, and update the last position on the read aloud
     * item in the menu.
     */
    private void maybeFindAndRemoveReadAloudItem() {
        if (mModelList == null) {
            return;
        }
        Iterator<ListItem> it = mModelList.iterator();
        int counter = 0;
        while (it.hasNext()) {
            ListItem li = it.next();
            int id = li.model.get(AppMenuItemProperties.MENU_ITEM_ID);
            if (id == R.id.readaloud_menu_id) {
                mReadAloudPos = counter;
                mModelList.remove(li);
                mHasReadAloudInserted = false;
                return;
            }
            counter++;
        }
    }

    /** If ReadAloud is not present in modelList, insert it at the saved position. */
    private void maybeInsertReadAloudItem(ModelList modelList) {
        // Already on the list, return early
        if (mHasReadAloudInserted) {
            return;
        }

        // now try to insert it.
        assert mReadAloudPos != -1 : "Unexpectedly missing position for the read aloud menu item";
        if (mReadAloudPos != -1) {
            mHasReadAloudInserted = true;
            PropertyModel propertyModel =
                    buildModelForStandardMenuItem(
                            R.id.readaloud_menu_id,
                            R.string.menu_listen_to_this_page,
                            R.drawable.ic_play_circle);
            modelList.add(
                    mReadAloudPos,
                    new MVCListAdapter.ListItem(AppMenuItemType.STANDARD, propertyModel));
        }
    }

    /** Return whether the given {@link MenuItem} is managed by policy. */
    protected boolean isMenuItemManaged(@IdRes int itemId) {
        if (itemId == R.id.new_incognito_tab_menu_id) {
            return IncognitoUtils.isIncognitoModeManaged(
                    mTabModelSelector.getCurrentModel().getProfile());
        }
        return false;
    }

    /** Returns true if a badge (i.e. a red-dot) should be shown on the menu item icon. */
    protected boolean shouldShowBadgeOnMenuItemIcon(@IdRes int itemId) {
        if (itemId == R.id.preferences_id) {
            // Theoretically mTabModelSelector could return a stub model.
            Profile profile = mTabModelSelector.getCurrentModel().getProfile();
            if (profile == null) {
                return false;
            }
            // Return true if there is any identity error(for signed-in users) or sync error(for
            // syncing users).
            return SyncSettingsUtils.getIdentityError(profile)
                            != SyncSettingsUtils.SyncError.NO_ERROR
                    || SyncSettingsUtils.getSyncError(profile)
                            != SyncSettingsUtils.SyncError.NO_ERROR;
        }
        return false;
    }

    /**
     * Returns content description for the menu item, if different from the titleCondensed xml
     * attribute.
     */
    protected String getContentDescription(@IdRes int itemId) {
        if (itemId == R.id.preferences_id) {
            // Theoretically mTabModelSelector could return a stub model.
            Profile profile = mTabModelSelector.getCurrentModel().getProfile();
            if (profile == null) {
                return null;
            }
            if (SyncSettingsUtils.getIdentityError(profile) != SyncSettingsUtils.SyncError.NO_ERROR
                    || SyncSettingsUtils.getSyncError(profile)
                            != SyncSettingsUtils.SyncError.NO_ERROR) {
                return mContext.getString(R.string.menu_settings_account_error);
            }
        }
        return null;
    }

    @Override
    public void loadingStateChanged(boolean isLoading) {
        if (mModelList == null) return;

        for (ListItem listItem : mModelList) {
            if (listItem.model.get(AppMenuItemProperties.MENU_ITEM_ID) == R.id.icon_row_menu_id) {
                ModelList subList = listItem.model.get(AppMenuItemProperties.ADDITIONAL_ICONS);
                for (ListItem subListItem : subList) {
                    if (subListItem.model.get(AppMenuItemProperties.MENU_ITEM_ID)
                            == R.id.reload_menu_id) {
                        updateReloadPropertyModel(subListItem.model, isLoading);
                        return;
                    }
                }
            }
        }
    }

    /**
     * Updates the given model based on the current loading state of the tab.
     *
     * @param reloadPropertyModel The property model associated with the reload action.
     * @param isLoading Whether the current tab is loading or not.
     */
    protected void updateReloadPropertyModel(PropertyModel reloadPropertyModel, boolean isLoading) {
        Resources resources = mContext.getResources();
        reloadPropertyModel
                .get(AppMenuItemProperties.ICON)
                .setLevel(
                        isLoading
                                ? resources.getInteger(R.integer.reload_button_level_stop)
                                : resources.getInteger(R.integer.reload_button_level_reload));
        reloadPropertyModel.set(
                AppMenuItemProperties.TITLE,
                resources.getString(
                        isLoading
                                ? R.string.accessibility_btn_stop_loading
                                : R.string.accessibility_btn_refresh));
        reloadPropertyModel.set(
                AppMenuItemProperties.TITLE_CONDENSED,
                resources.getString(isLoading ? R.string.menu_stop_refresh : R.string.refresh));
    }

    @Override
    public void onMenuDismissed() {}

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
    public boolean isMenuIconAtStart() {
        return false;
    }

    /**
     * Updates the bookmark item's visibility.
     *
     * @param bookmarkMenuItemShortcut {@link MenuItem} for adding/editing the bookmark.
     *     TODO(crbug.com/40145539): Remove this param and only pass in a non-null property model.
     * @param bookmarkMenuModel The {@link PropertyModel} associated with the bookmark item being
     *     updated.
     * @param currentTab Current tab being displayed.
     */
    protected void updateBookmarkMenuItemShortcut(
            @Nullable MenuItem bookmarkMenuItemShortcut,
            @Nullable PropertyModel bookmarkMenuModel,
            @Nullable Tab currentTab) {
        if (!mBookmarkModelSupplier.hasValue() || currentTab == null) {
            // If the BookmarkModel still isn't available, assume the bookmark menu item is not
            // editable.
            if (bookmarkMenuItemShortcut != null) {
                bookmarkMenuItemShortcut.setEnabled(false);
            }
            if (bookmarkMenuModel != null) {
                bookmarkMenuModel.set(AppMenuItemProperties.ENABLED, false);
            }
        } else {
            if (bookmarkMenuItemShortcut != null) {
                bookmarkMenuItemShortcut.setEnabled(
                        mBookmarkModelSupplier.get().isEditBookmarksEnabled());
            } else if (bookmarkMenuModel != null) {
                bookmarkMenuModel.set(
                        AppMenuItemProperties.ENABLED,
                        mBookmarkModelSupplier.get().isEditBookmarksEnabled());
            }
        }

        if (currentTab != null && shouldCheckBookmarkStar(currentTab)) {
            if (bookmarkMenuItemShortcut != null) {
                bookmarkMenuItemShortcut.setIcon(R.drawable.btn_star_filled);
                bookmarkMenuItemShortcut.setChecked(true);
                bookmarkMenuItemShortcut.setTitleCondensed(
                        mContext.getString(R.string.edit_bookmark));
            } else if (bookmarkMenuModel != null) {
                bookmarkMenuModel.set(
                        AppMenuItemProperties.ICON,
                        AppCompatResources.getDrawable(mContext, R.drawable.btn_star_filled));
                bookmarkMenuModel.set(AppMenuItemProperties.CHECKED, true);
                bookmarkMenuModel.set(
                        AppMenuItemProperties.TITLE_CONDENSED,
                        mContext.getString(R.string.edit_bookmark));
            }
        } else {
            if (bookmarkMenuItemShortcut != null) {
                bookmarkMenuItemShortcut.setIcon(R.drawable.star_outline_24dp);
                bookmarkMenuItemShortcut.setChecked(false);
                bookmarkMenuItemShortcut.setTitleCondensed(
                        mContext.getString(R.string.menu_bookmark));
            } else if (bookmarkMenuModel != null) {
                bookmarkMenuModel.set(
                        AppMenuItemProperties.ICON,
                        AppCompatResources.getDrawable(mContext, R.drawable.star_outline_24dp));
                bookmarkMenuModel.set(AppMenuItemProperties.CHECKED, false);
                bookmarkMenuModel.set(
                        AppMenuItemProperties.TITLE_CONDENSED,
                        mContext.getString(R.string.menu_bookmark));
            }
        }
    }

    /**
     * Updates the price-tracking menu item visibility.
     *
     * @param startPriceTrackingMenuItem The menu item to start price tracking.
     * @param stopPriceTrackingMenuItem The menu item to stop price tracking.
     * @param currentTab Current tab being displayed.
     */
    // TODO(crbug.com/40145539): Delete this method after inflation from XML is removed.
    protected void updatePriceTrackingMenuItemRow(
            @NonNull MenuItem startPriceTrackingMenuItem,
            @NonNull MenuItem stopPriceTrackingMenuItem,
            @Nullable Tab currentTab) {
        if (currentTab == null || currentTab.getWebContents() == null) {
            startPriceTrackingMenuItem.setVisible(false);
            stopPriceTrackingMenuItem.setVisible(false);
            return;
        }

        Profile profile = currentTab.getProfile();
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        ShoppingService.ProductInfo info = null;
        if (service != null) {
            info = service.getAvailableProductInfoForUrl(currentTab.getUrl());
        }

        // If price tracking isn't enabled or the page isn't eligible, then hide both items.
        if (!CommerceFeatureUtils.isShoppingListEligible(service)
                || !PowerBookmarkUtils.isPriceTrackingEligible(currentTab)
                || !mBookmarkModelSupplier.hasValue()) {
            startPriceTrackingMenuItem.setVisible(false);
            stopPriceTrackingMenuItem.setVisible(false);
            return;
        }

        boolean editEnabled = mBookmarkModelSupplier.get().isEditBookmarksEnabled();
        startPriceTrackingMenuItem.setEnabled(editEnabled);
        stopPriceTrackingMenuItem.setEnabled(editEnabled);

        if (info != null && info.productClusterId.isPresent()) {
            CommerceSubscription sub =
                    new CommerceSubscription(
                            SubscriptionType.PRICE_TRACK,
                            IdentifierType.PRODUCT_CLUSTER_ID,
                            UnsignedLongs.toString(info.productClusterId.get()),
                            ManagementType.USER_MANAGED,
                            null);
            boolean isSubscribed = service.isSubscribedFromCache(sub);
            startPriceTrackingMenuItem.setVisible(!isSubscribed);
            stopPriceTrackingMenuItem.setVisible(isSubscribed);
        } else {
            startPriceTrackingMenuItem.setVisible(true);
            stopPriceTrackingMenuItem.setVisible(false);
        }
    }

    /**
     * Builds the appropriate price tracking menu item for the current tab (if any).
     *
     * @param currentTab The currently selected tab.
     * @param showIcon Whether icons should be shown for this menu item.
     * @return The price tracking item appropriate for the current conditions (if any).
     */
    @Nullable
    protected ListItem maybeBuildPriceTrackingListItem(Tab currentTab, boolean showIcon) {
        if (currentTab.getWebContents() == null) return null;

        Profile profile = currentTab.getProfile();
        ShoppingService service = ShoppingServiceFactory.getForProfile(profile);
        ShoppingService.ProductInfo info = null;
        if (service != null) {
            info = service.getAvailableProductInfoForUrl(currentTab.getUrl());
        }

        // If price tracking isn't enabled or the page isn't eligible, then hide both items.
        if (!CommerceFeatureUtils.isShoppingListEligible(service)
                || !PowerBookmarkUtils.isPriceTrackingEligible(currentTab)
                || !mBookmarkModelSupplier.hasValue()) {
            return null;
        }

        boolean showStartPriceTracking = true;
        if (info != null && info.productClusterId.isPresent()) {
            CommerceSubscription sub =
                    new CommerceSubscription(
                            SubscriptionType.PRICE_TRACK,
                            IdentifierType.PRODUCT_CLUSTER_ID,
                            UnsignedLongs.toString(info.productClusterId.get()),
                            ManagementType.USER_MANAGED,
                            null);
            boolean isSubscribed = service.isSubscribedFromCache(sub);
            showStartPriceTracking = !isSubscribed;
        }

        if (showStartPriceTracking) {
            return new ListItem(
                    AppMenuItemType.STANDARD,
                    buildModelForStandardMenuItem(
                            R.id.enable_price_tracking_menu_id,
                            R.string.enable_price_tracking_menu_item,
                            showIcon ? R.drawable.price_tracking_disabled : 0));
        } else {
            return new ListItem(
                    AppMenuItemType.STANDARD,
                    buildModelForStandardMenuItem(
                            R.id.disable_price_tracking_menu_id,
                            R.string.disable_price_tracking_menu_item,
                            showIcon ? R.drawable.price_tracking_enabled_filled : 0));
        }
    }

    /**
     * Updates the request desktop site item's state.
     *
     * @param menu {@link Menu} for request desktop site.
     * @param currentTab Current tab being displayed.
     * @param canShowRequestDesktopSite If the request desktop site menu item should show or not.
     * @param isNativePage Whether the current tab is a native page.
     */
    // TODO(crbug.com/40145539): Delete this method after inflation from XML is removed.
    protected void updateRequestDesktopSiteMenuItem(
            Menu menu,
            @Nullable Tab currentTab,
            boolean canShowRequestDesktopSite,
            boolean isNativePage) {
        MenuItem requestMenuRow = menu.findItem(R.id.request_desktop_site_row_menu_id);
        MenuItem requestMenuLabel = menu.findItem(R.id.request_desktop_site_id);
        MenuItem requestMenuCheck = menu.findItem(R.id.request_desktop_site_check_id);

        // Hide request desktop site on all native pages. Also hide it for desktop Android, which
        // always requests desktop sites.
        boolean itemVisible =
                currentTab != null
                        && canShowRequestDesktopSite
                        && !isNativePage
                        && !shouldShowReaderModePrefs(currentTab)
                        && currentTab.getWebContents() != null
                        && !BuildConfig.IS_DESKTOP_ANDROID;

        requestMenuRow.setVisible(itemVisible);
        if (!itemVisible) return;

        boolean isRequestDesktopSite =
                currentTab.getWebContents().getNavigationController().getUseDesktopUserAgent();
        requestMenuLabel.setTitle(R.string.menu_request_desktop_site);
        requestMenuCheck.setVisible(true);
        // Mark the checkbox if RDS is activated on this page.
        requestMenuCheck.setChecked(isRequestDesktopSite);

        // This title doesn't seem to be displayed by Android, but it is used to set up
        // accessibility text in {@link AppMenuAdapter#setupMenuButton}.
        requestMenuLabel.setTitleCondensed(
                isRequestDesktopSite
                        ? mContext.getString(R.string.menu_request_desktop_site_on)
                        : mContext.getString(R.string.menu_request_desktop_site_off));
    }

    /**
     * Builds the appropriate RDS menu item for the current tab (if any).
     *
     * @param currentTab The currently selected tab.
     * @param isNativePage Whether the current page is showing a NativePage.
     * @param showIcon Whether icons should be shown for this menu item.
     * @return The RDS item appropriate for the current conditions (if any).
     */
    @Nullable
    protected ListItem maybeBuildRequestDesktopSiteListItem(
            Tab currentTab, boolean isNativePage, boolean showIcon) {
        // Hide request desktop site on all native pages. Also hide it for desktop Android, which
        // always requests desktop sites.
        boolean itemVisible =
                !isNativePage
                        && !shouldShowReaderModePrefs(currentTab)
                        && currentTab.getWebContents() != null
                        && !BuildConfig.IS_DESKTOP_ANDROID;

        if (!itemVisible) return null;

        boolean isRequestDesktopSite =
                currentTab.getWebContents().getNavigationController().getUseDesktopUserAgent();

        PropertyModel model =
                buildModelForMenuItemWithCheckbox(
                        R.id.request_desktop_site_id,
                        R.string.menu_request_desktop_site,
                        showIcon ? R.drawable.ic_desktop_windows : 0,
                        R.id.request_desktop_site_check_id,
                        isRequestDesktopSite);

        // This title doesn't seem to be displayed by Android, but it is used to set up
        // accessibility text in {@link AppMenuAdapter#setupMenuButton}.
        model.set(
                AppMenuItemProperties.TITLE_CONDENSED,
                isRequestDesktopSite
                        ? mContext.getString(R.string.menu_request_desktop_site_on)
                        : mContext.getString(R.string.menu_request_desktop_site_off));

        return new ListItem(AppMenuItemType.TITLE_BUTTON, model);
    }

    /**
     * Updates the auto dark menu item's state.
     *
     * @param menu {@link Menu} for auto dark.
     * @param currentTab Current tab being displayed.
     * @param isNativePage Whether the current tab is a native page.
     */
    protected void updateAutoDarkMenuItem(
            Menu menu, @Nullable Tab currentTab, boolean isNativePage) {
        MenuItem autoDarkMenuRow = menu.findItem(R.id.auto_dark_web_contents_row_menu_id);
        MenuItem autoDarkMenuCheck = menu.findItem(R.id.auto_dark_web_contents_check_id);

        // Hide app menu item if on non-NTP chrome:// page or auto dark not enabled.
        boolean isAutoDarkEnabled = isAutoDarkWebContentsEnabled();
        boolean itemVisible = currentTab != null && !isNativePage && isAutoDarkEnabled;
        autoDarkMenuRow.setVisible(itemVisible);
        if (!itemVisible) return;

        // Set text based on if site is blocked or not.
        boolean isEnabled =
                WebContentsDarkModeController.isEnabledForUrl(
                        mTabModelSelector.getCurrentModel().getProfile(), currentTab.getUrl());
        autoDarkMenuCheck.setChecked(isEnabled);
    }

    protected void updateManagedByMenuItem(Menu menu, @Nullable Tab currentTab) {
        MenuItem managedByDividerLine = menu.findItem(R.id.managed_by_divider_line_id);
        MenuItem managedByMenuItem = menu.findItem(R.id.managed_by_menu_id);

        boolean managedByMenuItemVisible =
                currentTab != null && shouldShowManagedByMenuItem(currentTab);

        managedByDividerLine.setVisible(managedByMenuItemVisible);
        managedByMenuItem.setVisible(managedByMenuItemVisible);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean isIncognitoEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled(
                mTabModelSelector.getCurrentModel().getProfile());
    }

    static void setPageBookmarkedForTesting(Boolean bookmarked) {
        sItemBookmarkedForTesting = bookmarked;
        ResettersForTesting.register(() -> sItemBookmarkedForTesting = null);
    }

    void setBookmarkModelSupplierForTesting(
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier) {
        mBookmarkModelSupplier = bookmarkModelSupplier;
    }

    /**
     * @return Whether the menu item's icon need to be tinted to blue.
     */
    protected @ColorRes int getMenuItemIconColorRes(@IdRes int itemId) {
        if (itemId == R.id.disable_price_tracking_menu_id) {
            return R.color.default_icon_color_accent1_tint_list;
        }
        return R.color.default_icon_color_secondary_tint_list;
    }

    /**
     * Set the icon and the title for the menu item used for direct share.
     *
     * @param item The menu item that is used for direct share.
     */
    // TODO(crbug.com/40145539): Delete this method after inflation from XML is removed.
    protected void updateDirectShareMenuItem(MenuItem item) {
        Pair<Drawable, CharSequence> directShare = ShareHelper.getShareableIconAndNameForText();
        Drawable directShareIcon = directShare.first;
        CharSequence directShareTitle = directShare.second;

        item.setIcon(directShareIcon);
        if (directShareTitle != null) {
            item.setTitle(
                    mContext.getString(R.string.accessibility_menu_share_via, directShareTitle));
        }
    }

    /**
     * Builds the appropriate share menu item.
     *
     * @param showIcon Whether icons should be shown for this menu item.
     * @return The share list item.
     */
    protected ListItem buildShareListItem(boolean showIcon) {
        Pair<Drawable, CharSequence> directShare = ShareHelper.getShareableIconAndNameForText();
        if (directShare.first != null) {
            CharSequence directShareTitle = directShare.second;
            if (directShareTitle != null) {
                directShareTitle =
                        mContext.getString(R.string.accessibility_menu_share_via, directShareTitle);
            }
            return new ListItem(
                    AppMenuItemType.TITLE_BUTTON,
                    buildModelForMenuItemWithSecondaryButton(
                            R.id.share_menu_id,
                            R.string.menu_share_page,
                            showIcon ? R.drawable.ic_share_white_24dp : 0,
                            R.id.direct_share_menu_id,
                            directShareTitle,
                            directShare.first));
        } else {
            return new ListItem(
                    AppMenuItemType.STANDARD,
                    buildModelForStandardMenuItem(
                            R.id.share_menu_id,
                            R.string.menu_share_page,
                            showIcon ? R.drawable.ic_share_white_24dp : 0));
        }
    }

    /** Records user clicking on the menu button in New tab page. */
    @Override
    public void onMenuShown() {
        Tab currentTab = mActivityTabProvider.get();
        if (currentTab != null
                && UrlUtilities.isNtpUrl(currentTab.getUrl())
                && !currentTab.isIncognito()) {
            BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.MENU_BUTTON);
        }
        switch (getMenuGroup()) {
            case MenuGroup.PAGE_MENU:
                RecordUserAction.record("MobileMenuShow.PageMenu");
                break;
            case MenuGroup.OVERVIEW_MODE_MENU:
                RecordUserAction.record("MobileMenuShow.OverviewModeMenu");
                break;
            case MenuGroup.TABLET_EMPTY_MODE_MENU:
                RecordUserAction.record("MobileMenuShow.TabletEmptyModeMenu");
                break;
            case MenuGroup.INVALID: // fallthrough
            default:
                // Intentional noop.
        }
    }

    public @StringRes int getAddToGroupMenuItemString(@Nullable Token currentTabGroupId) {
        TabGroupModelFilter filter =
                mTabModelSelector.getTabGroupModelFilterProvider().getCurrentTabGroupModelFilter();
        if (currentTabGroupId != null) return R.string.menu_move_tab_to_group;
        if (filter != null) {
            boolean hasGroups = filter.getTabGroupCount() != 0;
            return hasGroups ? R.string.menu_add_tab_to_group : R.string.menu_add_tab_to_new_group;
        }
        return R.string.menu_add_tab_to_group;
    }
}
