// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.ntp.RecentlyClosedBulkEvent;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedGroup;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.ntp.TitleUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuRecentEntryItemProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

@NullMarked
public class HistoryItemBuilder implements Destroyable {
    public static final int MAX_RECENT_ENTRIES_TO_SHOW = 8;

    private final Context mContext;
    private final AppMenuItemTheme mAppMenuItemTheme;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<FaviconHelper> mFaviconHelperSupplier;
    private final Supplier<RecentlyClosedEntriesManager> mRecentlyClosedEntriesManagerSupplier;
    private final boolean mIsMenuIconAtStart;
    private final boolean mShouldShowIconBeforeItem;
    private final RoundedIconGenerator mRoundedIconGenerator;
    private final FaviconHelper.DefaultFaviconHelper mDefaultFaviconHelper;
    private @Nullable ForeignSessionHelper mForeignSessionHelper;

    /**
     * Constructs a {@link HistoryItemBuilder} which is responsible for building the History and
     * Recent Tabs menu items for the app menu.
     *
     * @param context The Android Context used to get resources.
     * @param appMenuItemTheme The theme used to style the app menu items.
     * @param tabModelSelector The selector used to query tab state.
     * @param faviconHelperSupplier Supplies the FaviconHelper.
     * @param recentlyClosedEntriesManagerSupplier Supplies the manager for recently closed entries.
     * @param isMenuIconAtStart Whether the menu icon is displayed at the start.
     * @param shouldShowIconBeforeItem Whether an icon should be shown before the item text.
     * @param roundedIconGenerator Generates rounded icons.
     * @param defaultFaviconHelper Helper for default favicons.
     */
    public HistoryItemBuilder(
            Context context,
            AppMenuItemTheme appMenuItemTheme,
            TabModelSelector tabModelSelector,
            Supplier<FaviconHelper> faviconHelperSupplier,
            Supplier<RecentlyClosedEntriesManager> recentlyClosedEntriesManagerSupplier,
            boolean isMenuIconAtStart,
            boolean shouldShowIconBeforeItem,
            RoundedIconGenerator roundedIconGenerator,
            FaviconHelper.DefaultFaviconHelper defaultFaviconHelper) {
        mContext = context;
        mAppMenuItemTheme = appMenuItemTheme;
        mTabModelSelector = tabModelSelector;
        mFaviconHelperSupplier = faviconHelperSupplier;
        mRecentlyClosedEntriesManagerSupplier = recentlyClosedEntriesManagerSupplier;
        mIsMenuIconAtStart = isMenuIconAtStart;
        mShouldShowIconBeforeItem = shouldShowIconBeforeItem;
        mRoundedIconGenerator = roundedIconGenerator;
        mDefaultFaviconHelper = defaultFaviconHelper;
    }

    /** Cleans up native resources and helpers used by this builder. */
    @Override
    public void destroy() {
        if (mForeignSessionHelper != null) {
            mForeignSessionHelper.destroy();
            mForeignSessionHelper = null;
        }
    }

    /**
     * Determines whether the "History" parent item should be shown.
     *
     * @return true if the history parent item should be shown, false otherwise.
     */
    public boolean shouldShowHistoryParentItem() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU)) {
            return false;
        }

        if (!IncognitoUtils.shouldOpenIncognitoAsWindow() && !isIncognitoShowing()) {
            return true;
        }

        if (shouldShowRecentTabsItem()) {
            return true;
        }

        return false;
    }

    /**
     * Builds the "History" parent item, populating its submenu with the standard History item,
     * Recent Tabs, recently closed entries, and foreign sessions.
     *
     * @return A {@link ListItem} representing the expandable History parent item.
     */
    public ListItem buildHistoryParentItem() {
        assert shouldShowHistoryParentItem();

        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();
                    if (!IncognitoUtils.shouldOpenIncognitoAsWindow() || !isIncognitoShowing()) {
                        submenuItems.add(buildHistoryItem(/* showIcon= */ false));
                    }

                    if (shouldShowRecentTabsItem()) {
                        submenuItems.add(buildRecentTabsItem(/* showIcon= */ false));
                    }

                    List<ListItem> recentEntries = getRecentEntryMenuItemList();
                    if (!recentEntries.isEmpty()) {
                        submenuItems.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.DIVIDER,
                                        AppMenuItemUtils.buildModelForDivider(
                                                R.id.divider_line_id)));
                        submenuItems.add(
                                AppMenuItemUtils.buildHeaderItem(
                                        mContext,
                                        mAppMenuItemTheme,
                                        R.id.recent_tabs_header_menu_id,
                                        R.string.recent_tabs,
                                        mIsMenuIconAtStart));
                        submenuItems.addAll(recentEntries);
                    }

                    List<ListItem> foreignSessions = new ArrayList<>();
                    for (ForeignSession session : getForeignSessionHelper().getForeignSessions()) {
                        foreignSessions.add(buildForeignSessionSubmenuItem(session));
                    }
                    if (!foreignSessions.isEmpty()) {
                        submenuItems.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.DIVIDER,
                                        AppMenuItemUtils.buildModelForDivider(
                                                R.id.divider_line_id)));
                        submenuItems.addAll(foreignSessions);
                    }

                    return submenuItems;
                };

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.history_parent_menu_id,
                        R.string.menu_history,
                        mShouldShowIconBeforeItem ? R.drawable.ic_history_24dp : Resources.ID_NULL,
                        submenuItemsSupplier,
                        mIsMenuIconAtStart));
    }

    /**
     * Builds the standalone "History" menu item.
     *
     * @param showIcon Whether to display an icon next to the item.
     * @return A {@link ListItem} representing the History menu item.
     */
    public ListItem buildHistoryItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.open_history_menu_id,
                        R.string.menu_history,
                        showIcon ? R.drawable.ic_history_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /**
     * Determines whether the standalone "Recent Tabs" item should be shown.
     *
     * @return true if the recent tabs item should be shown, false otherwise.
     */
    public boolean shouldShowRecentTabsItem() {
        return !isIncognitoShowing();
    }

    /**
     * Builds the standalone "Recent Tabs" menu item.
     *
     * @param showIcon Whether to display an icon next to the item.
     * @return A {@link ListItem} representing the Recent Tabs menu item.
     */
    public ListItem buildRecentTabsItem(boolean showIcon) {
        assert shouldShowRecentTabsItem();
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.recent_tabs_menu_id,
                        R.string.menu_recent_tabs,
                        showIcon ? R.drawable.devices_black_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    private List<ListItem> getRecentEntryMenuItemList() {
        List<ListItem> items = new ArrayList<>();
        RecentlyClosedEntriesManager manager = mRecentlyClosedEntriesManagerSupplier.get();
        if (manager == null) return items;

        manager.updateRecentlyClosedEntries();

        // TODO(crbug.com/509065810): Support updating the menu items dynamically when the
        // recently closed entries list changes while the menu is open.
        int count = 0;
        for (RecentlyClosedEntry entry : manager.getRecentlyClosedEntries()) {
            if (count >= MAX_RECENT_ENTRIES_TO_SHOW) {
                break;
            }

            if (entry instanceof RecentlyClosedTab tab) {
                items.add(buildRecentTabMenuItem(tab));
                count++;
            } else if (entry instanceof RecentlyClosedWindow window) {
                items.add(buildClosedWindowMenuItem(window));
                manager.preFetchTabsForWindow(window);
                count++;
            } else if (entry instanceof RecentlyClosedGroup group) {
                items.add(buildClosedGroupMenuItem(group));
                count++;
            } else if (entry instanceof RecentlyClosedBulkEvent bulkEvent) {
                for (RecentlyClosedTab tab : bulkEvent.getTabs()) {
                    if (count >= MAX_RECENT_ENTRIES_TO_SHOW) {
                        break;
                    }
                    items.add(buildRecentTabMenuItem(tab));
                    count++;
                }
            }
        }
        return items;
    }

    private ListItem buildForeignSessionSubmenuItem(ForeignSession session) {
        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();
                    for (ForeignSessionWindow window : session.windows) {
                        // TODO(crbug.com/509065811): Limit the number of tabs displayed.
                        for (ForeignSessionTab tab : window.tabs) {
                            submenuItems.add(buildForeignSessionTabMenuItem(session, tab));
                        }
                    }
                    if (submenuItems.isEmpty()) {
                        submenuItems.add(TabbedAppMenuPropertiesDelegate.buildEmptySubmenuItem());
                    }
                    return submenuItems;
                };

        PropertyModel model =
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.recent_entry_menu_item,
                        session.name,
                        mShouldShowIconBeforeItem
                                ? getIconForFormFactor(session.formFactor)
                                : Resources.ID_NULL,
                        submenuItemsSupplier,
                        mIsMenuIconAtStart);

        return AppMenuItemUtils.createMenuItemWithSubmenuListItem(model, mShouldShowIconBeforeItem);
    }

    private Profile getProfile() {
        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        assert profile != null;
        return profile;
    }

    private ForeignSessionHelper getForeignSessionHelper() {
        if (mForeignSessionHelper == null) {
            mForeignSessionHelper = new ForeignSessionHelper(getProfile());
        }
        return mForeignSessionHelper;
    }

    /* package */ void setForeignSessionHelperForTesting(ForeignSessionHelper helper) {
        mForeignSessionHelper = helper;
    }

    private @DrawableRes int getIconForFormFactor(@FormFactor int formFactor) {
        switch (formFactor) {
            case FormFactor.DESKTOP:
                return R.drawable.computer_black_24dp;
            case FormFactor.PHONE:
                return R.drawable.smartphone_black_24dp;
            case FormFactor.TABLET:
                return R.drawable.tablet_black_24dp;
            default:
                return R.drawable.devices_black_24dp;
        }
    }

    private ListItem buildForeignSessionTabMenuItem(ForeignSession session, ForeignSessionTab tab) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuRecentEntryItemProperties.ALL_KEYS)
                        .with(
                                AppMenuItemProperties.MENU_ITEM_ID,
                                R.id.recent_entry_foreign_tab_menu_item)
                        .with(AppMenuItemProperties.TITLE, tab.title)
                        .with(
                                AppMenuItemProperties.ICON_SUPPLIER,
                                TabbedAppMenuPropertiesDelegate.createIconSupplierForTab(
                                        mContext,
                                        /* faviconUrl= */ tab.url,
                                        /* tabGroupId= */ null,
                                        /* isOffTheRecord= */ false,
                                        /* cachedFavicon= */ null,
                                        /* fallbackToHost= */ false,
                                        mRoundedIconGenerator,
                                        mDefaultFaviconHelper,
                                        mFaviconHelperSupplier.get(),
                                        getProfile()))
                        .with(AppMenuItemProperties.ICON_NO_TINT, true)
                        .with(AppMenuItemProperties.ENABLED, true)
                        .with(AppMenuRecentEntryItemProperties.FOREIGN_SESSION_TAB, tab)
                        .with(AppMenuRecentEntryItemProperties.FOREIGN_SESSION_TAG, session.tag)
                        .build();
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY, model);
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
                                        AppMenuItemUtils.buildModelForDivider(
                                                R.id.divider_line_id)));
                        for (RecentlyClosedTab tab : tabs) {
                            submenuItems.add(
                                    buildClosedWindowTabMenuItem(tab, window.getInstanceId()));
                        }
                    }
                    return submenuItems;
                };

        PropertyModel model =
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.recent_entry_menu_item,
                        getRecentEntrySubmenuTitle(
                                window.getTitle().equals(RecentlyClosedWindow.WINDOW_DEFAULT_TITLE)
                                        ? null
                                        : window.getTitle(),
                                window.getTabCount()),
                        mShouldShowIconBeforeItem ? R.drawable.ic_window_24dp : Resources.ID_NULL,
                        submenuItemsSupplier,
                        mIsMenuIconAtStart);

        // TODO(crbug.com/521223427): Implement dynamic updates so we can re-enable this once the
        // model loads.
        model.set(AppMenuItemProperties.ENABLED, mTabModelSelector.isTabStateInitialized());

        return AppMenuItemUtils.createMenuItemWithSubmenuListItem(model, mShouldShowIconBeforeItem);
    }

    private ListItem buildRestoreWindowMenuItem(RecentlyClosedWindow window) {
        // TODO(crbug.com/521223427): Implement dynamic updates so we can re-enable this once the
        // model loads.
        PropertyModel model =
                AppMenuItemUtils.populateBaseModelForTextItem(
                                new PropertyModel.Builder(
                                        AppMenuRecentEntryItemProperties.ALL_KEYS),
                                mAppMenuItemTheme,
                                R.id.recent_entry_window_menu_item,
                                mIsMenuIconAtStart)
                        .with(
                                AppMenuItemProperties.TITLE,
                                mContext.getString(R.string.menu_recent_entry_restore_window))
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, window)
                        .with(AppMenuItemProperties.ICON, null)
                        .with(
                                AppMenuItemProperties.ENABLED,
                                mTabModelSelector.isTabStateInitialized())
                        .build();

        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY_NO_ICON, model);
    }

    private ListItem buildClosedWindowTabMenuItem(RecentlyClosedTab tab, int windowInstanceId) {
        // TODO(crbug.com/521223427): Implement dynamic updates so we can re-enable this once the
        // model loads.
        PropertyModel model =
                new PropertyModel.Builder(AppMenuRecentEntryItemProperties.ALL_KEYS)
                        .with(
                                AppMenuItemProperties.MENU_ITEM_ID,
                                R.id.recent_entry_window_tab_menu_item)
                        .with(AppMenuItemProperties.TITLE, tab.getTitle())
                        .with(
                                AppMenuItemProperties.ICON_SUPPLIER,
                                TabbedAppMenuPropertiesDelegate.createIconSupplierForTab(
                                        mContext,
                                        tab.getUrl(),
                                        /* tabGroupId= */ null,
                                        /* isOffTheRecord= */ false,
                                        /* cachedFavicon= */ null,
                                        /* fallbackToHost= */ false,
                                        mRoundedIconGenerator,
                                        mDefaultFaviconHelper,
                                        mFaviconHelperSupplier.get(),
                                        getProfile()))
                        .with(AppMenuItemProperties.ICON_NO_TINT, true)
                        .with(
                                AppMenuItemProperties.ENABLED,
                                mTabModelSelector.isTabStateInitialized())
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, tab)
                        .with(AppMenuRecentEntryItemProperties.WINDOW_ID, windowInstanceId)
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
                                    AppMenuItemUtils.buildModelForDivider(R.id.divider_line_id)));
                    for (RecentlyClosedTab tab : tabs) {
                        submenuItems.add(buildRecentTabMenuItem(tab));
                    }
                    return submenuItems;
                };

        PropertyModel model =
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.recent_entry_menu_item,
                        getRecentEntrySubmenuTitle(group.getTitle(), group.getTabs().size()),
                        mShouldShowIconBeforeItem
                                ? TabGroupItemBuilder.getTabGroupDrawable(
                                        mContext, getProfile().isOffTheRecord(), group.getColor())
                                : null,
                        submenuItemsSupplier,
                        mIsMenuIconAtStart);
        model.set(AppMenuItemProperties.ICON_NO_TINT, true);
        // TODO(crbug.com/521223427): Implement dynamic updates so we can re-enable this once the
        // model loads.
        model.set(AppMenuItemProperties.ENABLED, mTabModelSelector.isTabStateInitialized());

        return AppMenuItemUtils.createMenuItemWithSubmenuListItem(model, mShouldShowIconBeforeItem);
    }

    private ListItem buildRestoreGroupMenuItem(RecentlyClosedGroup group) {
        // TODO(crbug.com/521223427): Implement dynamic updates so we can re-enable this once the
        // model loads.
        PropertyModel model =
                AppMenuItemUtils.populateBaseModelForTextItem(
                                new PropertyModel.Builder(
                                        AppMenuRecentEntryItemProperties.ALL_KEYS),
                                mAppMenuItemTheme,
                                R.id.recent_entry_group_menu_item,
                                mIsMenuIconAtStart)
                        .with(
                                AppMenuItemProperties.TITLE,
                                mContext.getString(R.string.menu_recent_entry_restore_group))
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, group)
                        .with(AppMenuItemProperties.ICON, null)
                        .with(
                                AppMenuItemProperties.ENABLED,
                                mTabModelSelector.isTabStateInitialized())
                        .build();
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY_NO_ICON, model);
    }

    private ListItem buildRecentTabMenuItem(RecentlyClosedTab tab) {
        return buildRecentEntryMenuItem(
                tab,
                TitleUtil.getTitleForDisplay(tab.getTitle(), tab.getUrl()),
                TabbedAppMenuPropertiesDelegate.createIconSupplierForTab(
                        mContext,
                        tab.getUrl(),
                        tab.getTabGroupId(),
                        // Recently closed tabs are not tracked for incognito.
                        /* isOffTheRecord= */ false,
                        // No live Tab object is available to get a cached favicon.
                        /* cachedFavicon= */ null,
                        /* fallbackToHost= */ false,
                        mRoundedIconGenerator,
                        mDefaultFaviconHelper,
                        mFaviconHelperSupplier.get(),
                        getProfile()));
    }

    private ListItem buildRecentEntryMenuItem(
            RecentlyClosedEntry entry,
            String title,
            @Nullable LazyOneshotSupplier<Drawable> iconSupplier) {
        // TODO(crbug.com/521223427): Implement dynamic updates so we can re-enable this once the
        // model loads.
        PropertyModel.Builder builder =
                AppMenuItemUtils.populateBaseModelForTextItem(
                                new PropertyModel.Builder(
                                        AppMenuRecentEntryItemProperties.ALL_KEYS),
                                mAppMenuItemTheme,
                                R.id.recent_entry_tab_menu_item,
                                mIsMenuIconAtStart)
                        .with(AppMenuItemProperties.TITLE, title)
                        .with(AppMenuRecentEntryItemProperties.RECENT_ENTRY, entry)
                        .with(
                                AppMenuItemProperties.ENABLED,
                                mTabModelSelector.isTabStateInitialized());
        if (mShouldShowIconBeforeItem && iconSupplier != null) {
            builder.with(AppMenuItemProperties.ICON_SUPPLIER, iconSupplier);
            builder.with(AppMenuItemProperties.ICON_NO_TINT, true);
        }
        return new ListItem(AppMenuHandler.AppMenuItemType.RECENT_ENTRY, builder.build());
    }

    private boolean isIncognitoShowing() {
        return mTabModelSelector.getCurrentModel().isIncognito();
    }
}
