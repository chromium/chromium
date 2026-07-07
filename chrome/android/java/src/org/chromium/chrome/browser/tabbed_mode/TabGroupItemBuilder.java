// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.text.TextUtils;

import androidx.annotation.StringRes;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTabItemProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Builds AppMenu ListItems for TabGroup-related menus. */
@NullMarked
/* package */ class TabGroupItemBuilder {

    private final Context mContext;
    private final AppMenuItemTheme mAppMenuItemTheme;
    private final TabModelSelector mTabModelSelector;
    private final boolean mIsMenuIconAtStart;
    private final boolean mShouldShowIconBeforeItem;
    private final RoundedIconGenerator mRoundedIconGenerator;
    private final FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
    private final Supplier<FaviconHelper> mFaviconHelperSupplier;

    /**
     * Constructs a new {@link TabGroupItemBuilder}.
     *
     * @param context The current {@link Context}.
     * @param appMenuItemTheme The {@link AppMenuItemTheme} to use for styling.
     * @param tabModelSelector The {@link TabModelSelector} to get the TabModel.
     * @param isMenuIconAtStart Whether the menu icon should be shown at the start.
     * @param shouldShowIconBeforeItem Whether an icon should be shown before the item.
     * @param roundedIconGenerator Generates rounded icons for favicons.
     * @param defaultFaviconHelper Helper for default favicons.
     * @param faviconHelperSupplier Supplier for fetching favicons.
     */
    /* package */ TabGroupItemBuilder(
            Context context,
            AppMenuItemTheme appMenuItemTheme,
            TabModelSelector tabModelSelector,
            boolean isMenuIconAtStart,
            boolean shouldShowIconBeforeItem,
            RoundedIconGenerator roundedIconGenerator,
            FaviconHelper.DefaultFaviconHelper defaultFaviconHelper,
            Supplier<FaviconHelper> faviconHelperSupplier) {
        mContext = context;
        mAppMenuItemTheme = appMenuItemTheme;
        mTabModelSelector = tabModelSelector;
        mIsMenuIconAtStart = isMenuIconAtStart;
        mShouldShowIconBeforeItem = shouldShowIconBeforeItem;
        mRoundedIconGenerator = roundedIconGenerator;
        mDefaultFaviconHelper = defaultFaviconHelper;
        mFaviconHelperSupplier = faviconHelperSupplier;
    }

    /**
     * Determines whether the "Tab groups" parent menu item should be shown.
     *
     * @param currentTab The currently active {@link Tab}, or null if none is active.
     * @return true if the item should be shown, false otherwise.
     */
    /* package */ boolean shouldShowTabGroupsParentItem(@Nullable Tab currentTab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            return false;
        }
        return shouldShowAddToGroup() || currentTab != null;
    }

    /**
     * Builds a {@link ListItem} for the "Tab groups" parent menu item, containing submenus.
     *
     * @param currentTab The currently active {@link Tab}, or null if none is active.
     * @return The built {@link ListItem}.
     */
    /* package */ ListItem buildTabGroupsParentItem(@Nullable Tab currentTab) {
        assert shouldShowTabGroupsParentItem(currentTab);
        boolean showIcon = mShouldShowIconBeforeItem;
        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.tab_groups_parent_menu_id,
                        R.string.menu_tab_groups,
                        showIcon ? R.drawable.ic_widgets : Resources.ID_NULL,
                        () -> buildSubmenuForTabGroupsParent(currentTab, showIcon),
                        mIsMenuIconAtStart));
    }

    /**
     * Determines whether the "Add to group" menu item should be shown.
     *
     * @return true if the item should be shown, false otherwise.
     */
    /* package */ boolean shouldShowAddToGroup() {
        return mTabModelSelector.isTabStateInitialized();
    }

    /**
     * Builds a {@link ListItem} for the "Add to group" menu item.
     *
     * @param currentTab The currently active {@link Tab}, or null if none is active.
     * @param showIcon Whether to show an icon for the menu item.
     * @return The built {@link ListItem}.
     */
    /* package */ ListItem buildAddToGroupItem(@Nullable Tab currentTab, boolean showIcon) {
        assert shouldShowAddToGroup();
        PropertyModel model =
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.add_to_group_menu_id,
                        R.string.menu_add_tab_to_group,
                        showIcon ? R.drawable.ic_widgets : 0,
                        mIsMenuIconAtStart);
        model.set(
                AppMenuItemProperties.TITLE,
                mContext.getString(
                        getAddToGroupMenuItemString(
                                currentTab != null ? currentTab.getTabGroupId() : null)));
        return AppMenuItemUtils.createStandardListItem(model, showIcon);
    }

    /**
     * Builds a {@link ListItem} for the "New tab group" menu item. This is used in the tab switcher
     * to create an empty tab group.
     *
     * @return The built {@link ListItem}.
     */
    /* package */ ListItem buildNewTabGroupItemWithoutTab() {
        return new ListItem(
                AppMenuHandler.AppMenuItemType.STANDARD,
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.new_tab_group_menu_id,
                        R.string.menu_new_tab_group,
                        mShouldShowIconBeforeItem ? R.drawable.ic_widgets : Resources.ID_NULL,
                        mIsMenuIconAtStart));
    }

    /**
     * Builds a {@link ListItem} for the "Create new tab group" menu item. This is used in the
     * tab-level menu to create a new group containing the current tab.
     *
     * @param showIcon Whether to show an icon for the menu item.
     * @return The built {@link ListItem}.
     */
    private ListItem buildNewTabGroupItemWithTab(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.create_new_tab_group_menu_id,
                        R.string.menu_create_new_tab_group,
                        showIcon ? R.drawable.ic_library_add_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /**
     * Builds the submenu items for the top-level "Tab groups" item. This includes actions like
     * "Create new tab group" and a list of all existing tab groups.
     *
     * @param currentTab The currently active {@link Tab}, or null if none is active.
     * @param showIcons Whether to show icons for the menu items.
     * @return A list of {@link ListItem}s for the submenu.
     */
    private List<ListItem> buildSubmenuForTabGroupsParent(
            @Nullable Tab currentTab, boolean showIcons) {
        List<ListItem> submenuItems = new ArrayList<>();
        if (currentTab != null) {
            submenuItems.add(buildNewTabGroupItemWithTab(/* showIcon= */ false));
        }

        if (shouldShowAddToGroup()) {
            submenuItems.add(buildAddToGroupItem(currentTab, /* showIcon= */ false));
        }

        TabModel tabModel = mTabModelSelector.getCurrentModel();
        Set<Token> groupIds = tabModel.getAllTabGroupIds();
        if (groupIds.isEmpty()) {
            if (submenuItems.isEmpty()) {
                submenuItems.add(AppMenuItemUtils.buildEmptySubmenuItem());
            }
            return submenuItems;
        }

        submenuItems.add(
                new ListItem(
                        AppMenuHandler.AppMenuItemType.DIVIDER,
                        AppMenuItemUtils.buildModelForDivider(R.id.divider_line_id)));
        submenuItems.add(
                AppMenuItemUtils.buildHeaderItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.tab_groups_header_menu_id,
                        R.string.menu_tab_groups,
                        mIsMenuIconAtStart));

        // TODO(crbug.com/509065807): Observe TabModel to update this while the menu is open.
        for (Token groupId : groupIds) {
            String title = tabModel.getTabGroupTitle(groupId);
            if (TextUtils.isEmpty(title)) {
                title =
                        TabGroupTitleUtils.getDefaultTitle(
                                mContext, tabModel.getTabCountForGroup(groupId));
            }

            PropertyModel model =
                    AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                            mContext,
                            mAppMenuItemTheme,
                            R.id.tab_group_menu_item_id,
                            title,
                            showIcons
                                    ? getTabGroupDrawable(
                                            mContext,
                                            tabModel.isIncognito(),
                                            tabModel.getTabGroupColorWithFallback(groupId))
                                    : null,
                            () -> buildSubmenuForSpecificGroup(groupId, tabModel),
                            mIsMenuIconAtStart);
            model.set(AppMenuItemProperties.ICON_NO_TINT, true);

            submenuItems.add(AppMenuItemUtils.createMenuItemWithSubmenuListItem(model, showIcons));
        }

        return submenuItems;
    }

    /**
     * Builds the submenu items for a specific tab group item. This contains a list of all the tabs
     * inside the given tab group.
     *
     * @param groupId The ID of the tab group.
     * @param tabModel The current {@link TabModel}.
     * @return A list of {@link ListItem}s representing the tabs in the group.
     */
    private List<ListItem> buildSubmenuForSpecificGroup(Token groupId, TabModel tabModel) {
        List<ListItem> submenuItems = new ArrayList<>();
        List<Tab> tabs = tabModel.getTabsInGroup(groupId);
        Profile profile = tabModel.getProfile();
        assert profile != null;
        for (Tab tab : tabs) {
            PropertyModel model =
                    AppMenuItemUtils.populateBaseModelForTextItem(
                                    new PropertyModel.Builder(AppMenuTabItemProperties.ALL_KEYS),
                                    mAppMenuItemTheme,
                                    R.id.tab_group_tab_menu_item,
                                    mIsMenuIconAtStart)
                            .with(AppMenuItemProperties.TITLE, tab.getTitle())
                            .with(AppMenuTabItemProperties.TAB_ID, tab.getId())
                            .with(
                                    AppMenuItemProperties.ICON_SUPPLIER,
                                    AppMenuItemUtils.createIconSupplierForTab(
                                            mContext,
                                            tab.getUrl(),
                                            tab.getTabGroupId(),
                                            tab.isOffTheRecord(),
                                            TabFavicon.getBitmap(tab),
                                            /* fallbackToHost= */ false,
                                            mRoundedIconGenerator,
                                            mDefaultFaviconHelper,
                                            mFaviconHelperSupplier.get(),
                                            profile))
                            .build();
            submenuItems.add(new ListItem(AppMenuHandler.AppMenuItemType.TAB, model));
        }
        return submenuItems;
    }

    private @StringRes int getAddToGroupMenuItemString(@Nullable Token currentTabGroupId) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (currentTabGroupId != null) return R.string.menu_move_tab_to_group;
        boolean hasGroups = tabModel.getTabGroupCount() != 0;
        return hasGroups ? R.string.menu_add_tab_to_group : R.string.menu_add_tab_to_new_group;
    }

    /**
     * Helper to get a drawable for a tab group.
     *
     * @param context The current {@link Context}.
     * @param isIncognito Whether the tab group is incognito.
     * @param color The color of the tab group.
     * @return The {@link Drawable} representing the tab group color.
     */
    /* package */ static Drawable getTabGroupDrawable(
            Context context, boolean isIncognito, @TabGroupColorId int color) {
        int circleSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.tab_group_nested_menu_color_icon_size);
        int iconSize =
                context.getResources()
                        .getDimensionPixelSize(org.chromium.ui.R.dimen.list_menu_item_icon_size);
        Drawable colorDrawable =
                TabGroupUtils.createColorDrawableForMenu(context, color, isIncognito, circleSize);
        int inset = (iconSize - circleSize) / 2;
        return new InsetDrawable(
                colorDrawable, /* leftInset= */ 0, inset, /* rightInset= */ 0, inset);
    }
}
