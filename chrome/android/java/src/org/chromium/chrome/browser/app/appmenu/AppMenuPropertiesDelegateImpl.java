// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import com.google.common.primitives.UnsignedLongs;

import org.chromium.base.CallbackController;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.sync.UserActionableError;
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
import java.util.function.Supplier;

/**
 * Base implementation of {@link AppMenuPropertiesDelegate} that handles hiding and showing menu
 * items based on activity state.
 */
@NullMarked
public abstract class AppMenuPropertiesDelegateImpl implements AppMenuPropertiesDelegate {
    private static @Nullable Boolean sItemBookmarkedForTesting;

    protected final Context mContext;
    protected final boolean mIsTablet;
    protected final ActivityTabProvider mActivityTabProvider;
    protected final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    protected final TabModelSelector mTabModelSelector;
    protected final ToolbarManager mToolbarManager;
    protected final View mDecorView;
    protected final Supplier<ReadAloudController> mReadAloudControllerSupplier;

    private CallbackController mCallbackController = new CallbackController();
    private ObservableSupplier<BookmarkModel> mBookmarkModelSupplier;
    private @Nullable ModelList mModelList;
    private int mReadAloudPos;
    protected @Nullable Runnable mReadAloudAppMenuResetter;
    private boolean mHasReadAloudInserted;

    @VisibleForTesting
    @IntDef({
        MenuGroup.INVALID,
        MenuGroup.PAGE_MENU,
        MenuGroup.OVERVIEW_MODE_MENU,
        MenuGroup.TABLET_EMPTY_MODE_MENU
    })
    @Retention(RetentionPolicy.SOURCE)
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
            Supplier<ReadAloudController> readAloudControllerSupplier) {
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

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
        ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
        if (readAloudController != null) {
            readAloudController.removeReadabilityUpdateListener(mReadAloudAppMenuResetter);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public @Nullable Runnable getReadAloudmenuResetter() {
        return mReadAloudAppMenuResetter;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public @Nullable ModelList getModelList() {
        return mModelList;
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
    public final ModelList getMenuItems() {
        mReadAloudPos = -1;
        mHasReadAloudInserted = false;
        mModelList = buildMenuModelList();
        return mModelList;
    }

    /** Construct the ModelList for the appropriate current state of the menu. */
    @VisibleForTesting
    public abstract ModelList buildMenuModelList();

    /**
     * Builds a property model for a divider item type.
     *
     * @param id The id of the divider.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForDivider(@IdRes int id) {
        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .build();
    }

    /**
     * Constructs the basis for text menu items models.
     *
     * @param id The id of the text menu item.
     * @return A Builder object that forms the basis for text menu item models.
     */
    public PropertyModel.Builder buildBaseModelForTextItem(@IdRes int id) {
        return populateBaseModelForTextItem(
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS), id);
    }

    /**
     * Populates the PropertyModel.Builder with the common properties for a text menu item.
     *
     * @param builder The builder to populate with data.
     * @param id The id of the text menu item.
     * @return A Builder object that forms the basis for text menu item models.
     */
    public PropertyModel.Builder populateBaseModelForTextItem(
            PropertyModel.Builder builder, @IdRes int id) {
        return builder.with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .with(AppMenuItemProperties.ENABLED, true)
                .with(AppMenuItemProperties.ICON_COLOR_RES, getMenuItemIconColorRes(id))
                .with(AppMenuItemProperties.ICON_SHOW_BADGE, shouldShowBadgeOnMenuItemIcon(id))
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
     * Build a property model for a menu item with submenu.
     *
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param submenuItems The list of {@code ListItem}s in the submenu.
     * @return The property model for this item.
     */
    public PropertyModel buildModelForMenuItemWithSubmenu(
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            List<ListItem> submenuItems) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemWithSubmenuProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                        .with(AppMenuItemProperties.TITLE, mContext.getString(titleId))
                        .with(AppMenuItemProperties.ENABLED, true)
                        .with(AppMenuItemProperties.ICON_COLOR_RES, getMenuItemIconColorRes(id))
                        .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart())
                        .with(AppMenuItemProperties.MANAGED, isMenuItemManaged(id))
                        .with(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS, submenuItems)
                        .with(
                                AppMenuItemProperties.ICON_SHOW_BADGE,
                                shouldShowBadgeOnMenuItemIcon(id))
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
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the reader mode preferences menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowReaderModePrefs(@Nullable Tab currentTab) {
        return currentTab != null
                && DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl())
                && !DomDistillerFeatures.sReaderModeDistillInApp.isEnabled();
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether reader mode is currently showing.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean isReaderModeShowing(@Nullable Tab currentTab) {
        return currentTab != null && DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl());
    }

    /** Construct the reader mode menu item. */
    protected MVCListAdapter.ListItem buildReaderModeItem(Tab currentTab) {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.reader_mode_menu_id,
                        DomDistillerUrlUtils.isDistilledPage(currentTab.getUrl())
                                ? R.string.hide_reading_mode_text
                                : R.string.show_reading_mode_text,
                        shouldShowIconBeforeItem() ? R.drawable.ic_mobile_friendly_24dp : 0));
    }

    /** Construct the reader mode preferences menu item. */
    protected ListItem buildReaderModePrefsItem() {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.reader_mode_prefs_id,
                        R.string.menu_reader_mode_prefs,
                        R.drawable.reader_mode_prefs_icon));
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the {@code currentTab} may be downloaded, indicating whether the download
     *     page menu item should be enabled.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldEnableDownloadPage(@Nullable Tab currentTab) {
        return DownloadUtils.isAllowedToDownloadPage(currentTab);
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether bookmark page menu item should be checked, indicating that the current tab
     *         is bookmarked.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldCheckBookmarkStar(Tab currentTab) {
        if (sItemBookmarkedForTesting != null) return sItemBookmarkedForTesting;

        var bookmarkModel = mBookmarkModelSupplier.get();
        if (bookmarkModel == null) return false;
        return bookmarkModel.hasBookmarkIdForTab(currentTab);
    }

    @VisibleForTesting
    public boolean instanceSwitcherWithMultiInstanceEnabled() {
        return MultiWindowUtils.instanceSwitcherEnabled()
                && MultiWindowUtils.isMultiInstanceApi31Enabled();
    }

    @VisibleForTesting
    public boolean isTabletSizeScreen() {
        return mIsTablet;
    }

    /**
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the currentTab should show an app menu item that requires a webContents. This
     *     will return false for native NTP, and true otherwise.
     */
    protected boolean shouldShowWebContentsDependentMenuItem(Tab currentTab) {
        return !currentTab.isNativePage() && currentTab.getWebContents() != null;
    }

    /**
     * This method should only be called once per context menu shown.
     *
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the translate menu item should be displayed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowTranslateMenuItem(@Nullable Tab currentTab) {
        return currentTab != null && TranslateUtils.canTranslateCurrentTab(currentTab, true);
    }

    /** Construct the translate menu item for the given tab. */
    protected ListItem buildTranslateMenuItem(Tab currentTab, boolean showIcon) {
        assert shouldShowTranslateMenuItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.translate_id,
                        R.string.menu_translate,
                        showIcon ? R.drawable.ic_translate : 0));
    }

    /** Return whether the current tab should show the "Open with..." item. */
    protected boolean shouldShowOpenWithItem(@Nullable Tab currentTab) {
        return currentTab != null
                && currentTab.isNativePage()
                && assumeNonNull(currentTab.getNativePage()).isPdf();
    }

    /** Construct the "Open with..." item for the given tab. */
    protected ListItem buildOpenWithItem(Tab currentTab, boolean showIcon) {
        assert shouldShowOpenWithItem(currentTab);
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                buildModelForStandardMenuItem(
                        R.id.open_with_id,
                        R.string.menu_open_with,
                        showIcon ? R.drawable.ic_open_in_new : 0));
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
            GURL url) {
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
     * Returns whether the "Download page" menu item should be displayed.
     *
     * @param currentTab Current tab being displayed.
     */
    protected boolean shouldShowDownloadPageMenuItem(Tab currentTab) {
        return isTabletSizeScreen() && shouldEnableDownloadPage(currentTab);
    }

    /** Build the PropertyModel for the forward navigation action. */
    protected PropertyModel buildForwardActionModel(@Nullable Tab currentTab) {
        PropertyModel forwardButton =
                buildModelForIcon(
                        R.id.forward_menu_id,
                        R.string.accessibility_menu_forward,
                        R.string.menu_forward,
                        R.drawable.btn_forward);
        forwardButton.set(
                AppMenuItemProperties.ENABLED, currentTab != null && currentTab.canGoForward());
        return forwardButton;
    }

    /** Build the PropertyModel for the bookmark this page action. */
    protected PropertyModel buildBookmarkActionModel(@Nullable Tab currentTab) {
        PropertyModel bookmarkButton =
                buildModelForIcon(
                        R.id.bookmark_this_page_id,
                        R.string.accessibility_menu_bookmark,
                        R.string.menu_bookmark,
                        0);
        updateBookmarkMenuItemShortcut(bookmarkButton, currentTab);
        return bookmarkButton;
    }

    /** Build the PropertyModel for the download this page action. */
    protected PropertyModel buildDownloadActionModel(@Nullable Tab currentTab) {
        PropertyModel downloadButton =
                buildModelForIcon(
                        R.id.offline_page_id,
                        R.string.download_page,
                        R.string.menu_download,
                        R.drawable.ic_file_download_white_24dp);
        downloadButton.set(AppMenuItemProperties.ENABLED, shouldEnableDownloadPage(currentTab));
        return downloadButton;
    }

    /** Build the PropertyModel for the page info action. */
    protected PropertyModel buildPageInfoModel(@Nullable Tab currentTab) {
        PropertyModel pageInfoButton =
                buildModelForIcon(
                        R.id.info_menu_id,
                        R.string.accessibility_menu_info,
                        R.string.menu_page_info,
                        R.drawable.btn_info);
        pageInfoButton.set(AppMenuItemProperties.ENABLED, currentTab != null);
        return pageInfoButton;
    }

    /** Build the PropertyModel for the reload/stop action. */
    protected PropertyModel buildReloadModel(@Nullable Tab currentTab) {
        PropertyModel reloadButton =
                buildModelForIcon(
                        R.id.reload_menu_id,
                        R.string.accessibility_btn_refresh,
                        R.string.refresh,
                        0);
        Drawable icon = AppCompatResources.getDrawable(mContext, R.drawable.btn_reload_stop);
        DrawableCompat.setTintList(
                icon,
                AppCompatResources.getColorStateList(
                        mContext, R.color.default_icon_color_tint_list));
        reloadButton.set(AppMenuItemProperties.ICON, icon);
        reloadButton.set(AppMenuItemProperties.ENABLED, currentTab != null);
        if (currentTab != null) updateReloadPropertyModel(reloadButton, currentTab.isLoading());
        return reloadButton;
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
            assumeNonNull(resolveInfo);
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

    public static @Nullable ResolveInfo queryWebApkResolveInfo(Context context, Tab currentTab) {
        String manifestId =
                AppBannerManager.maybeGetManifestId(assumeNonNull(currentTab.getWebContents()));
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
    public @Nullable Bundle getBundleForMenuItem(int itemId) {
        return null;
    }

    private void observeReadabilityUpdates(Tab currentTab) {
        ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
        if (readAloudController == null) return;

        if (mReadAloudAppMenuResetter == null) {
            mReadAloudAppMenuResetter =
                    () -> {
                        boolean isReadable = isTabReadable(currentTab);
                        if (isReadable) {
                            assumeNonNull(mModelList);
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
        if (itemId == R.id.new_incognito_tab_menu_id
                || itemId == R.id.new_incognito_window_menu_id) {
            return IncognitoUtils.isIncognitoModeManaged(
                    assumeNonNull(mTabModelSelector.getCurrentModel().getProfile()));
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
            // Return true if there is any error.
            return SyncSettingsUtils.getSyncError(profile) != UserActionableError.NONE;
        }
        return false;
    }

    /**
     * Returns content description for the menu item, if different from the titleCondensed xml
     * attribute.
     */
    protected @Nullable String getContentDescription(@IdRes int itemId) {
        if (itemId == R.id.preferences_id) {
            // Theoretically mTabModelSelector could return a stub model.
            Profile profile = mTabModelSelector.getCurrentModel().getProfile();
            if (profile == null) {
                return null;
            }
            if (SyncSettingsUtils.getSyncError(profile) != UserActionableError.NONE) {
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

                        // The additional icons model list is not observed, so replace the full
                        // list object to trigger an update.
                        ModelList replacementList = new ModelList();
                        replacementList.addAll(subList);
                        listItem.model.set(AppMenuItemProperties.ADDITIONAL_ICONS, replacementList);
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
    public @Nullable View buildFooterView(AppMenuHandler appMenuHandler) {
        return null;
    }

    @Override
    public @Nullable View buildHeaderView() {
        return null;
    }

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
     * @param bookmarkMenuModel The {@link PropertyModel} associated with the bookmark item being
     *     updated.
     * @param currentTab Current tab being displayed.
     */
    protected void updateBookmarkMenuItemShortcut(
            PropertyModel bookmarkMenuModel, @Nullable Tab currentTab) {
        var bookmarkModel = mBookmarkModelSupplier.get();
        if (bookmarkModel == null || currentTab == null) {
            // If the BookmarkModel still isn't available, assume the bookmark menu item is not
            // editable.
            bookmarkMenuModel.set(AppMenuItemProperties.ENABLED, false);
        } else {
            bookmarkMenuModel.set(
                    AppMenuItemProperties.ENABLED, bookmarkModel.isEditBookmarksEnabled());
        }

        if (currentTab != null && shouldCheckBookmarkStar(currentTab)) {
            bookmarkMenuModel.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, R.drawable.btn_star_filled));
            bookmarkMenuModel.set(AppMenuItemProperties.CHECKED, true);
            bookmarkMenuModel.set(
                    AppMenuItemProperties.TITLE_CONDENSED,
                    mContext.getString(R.string.edit_bookmark));
        } else {
            bookmarkMenuModel.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, R.drawable.star_outline_24dp));
            bookmarkMenuModel.set(AppMenuItemProperties.CHECKED, false);
            bookmarkMenuModel.set(
                    AppMenuItemProperties.TITLE_CONDENSED,
                    mContext.getString(R.string.menu_bookmark));
        }
    }

    /**
     * Builds the appropriate price tracking menu item for the current tab (if any).
     *
     * @param currentTab The currently selected tab.
     * @param showIcon Whether icons should be shown for this menu item.
     * @return The price tracking item appropriate for the current conditions (if any).
     */
    protected @Nullable ListItem maybeBuildPriceTrackingListItem(
            @Nullable Tab currentTab, boolean showIcon) {
        Boolean show = getPriceTrackingMenuItemInfo(currentTab);
        if (show == null) return null;

        if (show) {
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
     * Determine which menu to show for price tracking feature.
     *
     * @param currentTab The currently selected tab.
     * @return {@code true} to show 'enable'. Shows no option if {@code null}.
     */
    public @Nullable Boolean getPriceTrackingMenuItemInfo(@Nullable Tab currentTab) {
        if (currentTab == null || currentTab.getWebContents() == null) {
            return null;
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
                || mBookmarkModelSupplier.get() == null) {
            return null;
        }

        boolean showStartPriceTracking = true;
        if (info != null && info.productClusterId != null) {
            CommerceSubscription sub =
                    new CommerceSubscription(
                            SubscriptionType.PRICE_TRACK,
                            IdentifierType.PRODUCT_CLUSTER_ID,
                            UnsignedLongs.toString(info.productClusterId),
                            ManagementType.USER_MANAGED,
                            null);
            boolean isSubscribed = service.isSubscribedFromCache(sub);
            showStartPriceTracking = !isSubscribed;
        }

        return showStartPriceTracking;
    }

    /**
     * Builds the appropriate RDS menu item for the current tab (if any).
     *
     * @param currentTab The currently selected tab.
     * @param isNativePage Whether the current page is showing a NativePage.
     * @param showIcon Whether icons should be shown for this menu item.
     * @return The RDS item appropriate for the current conditions (if any).
     */
    protected @Nullable ListItem maybeBuildRequestDesktopSiteListItem(
            @Nullable Tab currentTab, boolean isNativePage, boolean showIcon) {
        // Hide request desktop site on all native pages. Also hide it for desktop Android, which
        // always requests desktop sites.
        boolean itemVisible =
                !isNativePage
                        && !shouldShowReaderModePrefs(currentTab)
                        && currentTab != null
                        && currentTab.getWebContents() != null
                        && !DeviceInfo.isDesktop();

        if (!itemVisible) return null;

        assumeNonNull(currentTab);
        assumeNonNull(currentTab.getWebContents());
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

    /** Return whether auto darkening is enabled for the current Tab. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public boolean shouldShowAutoDarkItem(@Nullable Tab currentTab, boolean isNativePage) {
        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        assert profile != null;
        boolean isFlagEnabled =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING);
        boolean isFeatureEnabled =
                WebContentsDarkModeController.isFeatureEnabled(mContext, profile);

        return currentTab != null && !isNativePage && isFlagEnabled && isFeatureEnabled;
    }

    /** Construct the ListItem for the auto darkening menu item. */
    protected ListItem buildAutoDarkItem(Tab currentTab, boolean isNativePage, boolean showIcon) {
        assert shouldShowAutoDarkItem(currentTab, isNativePage);
        boolean isEnabled =
                WebContentsDarkModeController.isEnabledForUrl(
                        assumeNonNull(mTabModelSelector.getCurrentModel().getProfile()),
                        currentTab.getUrl());
        return new ListItem(
                AppMenuItemType.TITLE_BUTTON,
                buildModelForMenuItemWithCheckbox(
                        R.id.auto_dark_web_contents_id,
                        R.string.menu_auto_dark_web_contents,
                        showIcon ? R.drawable.ic_brightness_medium_24dp : 0,
                        R.id.auto_dark_web_contents_check_id,
                        isEnabled));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public boolean isIncognitoEnabled() {
        return IncognitoUtils.isIncognitoModeEnabled(
                assumeNonNull(mTabModelSelector.getCurrentModel().getProfile()));
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
