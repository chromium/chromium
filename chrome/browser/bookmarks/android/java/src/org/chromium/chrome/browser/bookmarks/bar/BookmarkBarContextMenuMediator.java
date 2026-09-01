// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.PluralsRes;
import androidx.annotation.StringRes;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuAction;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarContextMenuMetrics.BookmarkBarContextMenuEntrypoint;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkBarVisibilityState;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Handles the business logic for building the Bookmarks Bar context menu. */
@NullMarked
class BookmarkBarContextMenuMediator {
    private final Context mContext;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Supplier<@Nullable Tab> mCurrentTabSupplier;
    private final BookmarkBarContextMenuDelegate mContextMenuDelegate;
    private final Runnable mDismissRunnable;
    private final NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;

    private @BookmarkBarContextMenuEntrypoint int mCurrentEntrypoint;

    /**
     * Constructs the bookmark bar context menu mediator.
     *
     * @param context The context for retrieving resources.
     * @param profileSupplier Used to access the active user profile.
     * @param currentTabSupplier Used to observe or retrieve the active tab.
     * @param contextMenuDelegate Delegate handling context menu actions.
     * @param dismissRunnable Runnable invoked to dismiss the popup menu.
     * @param xrSpaceModeObservableSupplier Used to check if currently in XR full space mode.
     */
    BookmarkBarContextMenuMediator(
            Context context,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<@Nullable Tab> currentTabSupplier,
            BookmarkBarContextMenuDelegate contextMenuDelegate,
            Runnable dismissRunnable,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mCurrentTabSupplier = currentTabSupplier;
        mContextMenuDelegate = contextMenuDelegate;
        mDismissRunnable = dismissRunnable;
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
    }

    ModelList buildContextMenuModelList(
            BookmarkItem item,
            BookmarkModel bookmarkModel,
            @BookmarkBarContextMenuEntrypoint int entrypoint) {
        mCurrentEntrypoint = entrypoint;
        final Profile profile = mProfileSupplier.get();
        if (profile == null) return new ModelList();

        final boolean isIncognito = profile.isOffTheRecord();
        final BookmarkId accountDesktopFolderId = bookmarkModel.getAccountDesktopFolderId();
        final boolean isBookmarksBarFolder =
                item.getId().equals(bookmarkModel.getDesktopFolderId())
                        || (accountDesktopFolderId != null
                                && item.getId().equals(accountDesktopFolderId));
        boolean canEditOrMoveOrDelete = !isBookmarksBarFolder;

        ModelList listItems = new ModelList();

        BookmarkId id = item.getId();
        BookmarkId parentId = item.isFolder() ? item.getId() : item.getParentId();

        if (item.isFolder()) {
            addFolderOpenOptions(listItems, id, bookmarkModel, isIncognito, item.getTitle());
        } else {
            addBookmarkOpenOptions(listItems, id, isIncognito);
        }

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        addCommonActions(listItems, id, parentId, isIncognito, canEditOrMoveOrDelete);
        addVisibilityControlActions(listItems, isIncognito);

        return listItems;
    }

    ModelList buildBookmarksBarEmptySpaceContextMenuModelList(BookmarkModel bookmarkModel) {
        mCurrentEntrypoint = BookmarkBarContextMenuEntrypoint.EMPTY_SPACE;
        final Profile profile = mProfileSupplier.get();
        if (profile == null) return new ModelList();

        final boolean isIncognito = profile.isOffTheRecord();
        ModelList listItems = new ModelList();

        final BookmarkId accountDesktopFolderId = bookmarkModel.getAccountDesktopFolderId();
        BookmarkId parentId =
                assertNonNull(
                        accountDesktopFolderId != null
                                ? accountDesktopFolderId
                                : bookmarkModel.getDesktopFolderId());

        BookmarkItem parentItem = bookmarkModel.getBookmarkById(parentId);
        String parentTitle = parentItem != null ? parentItem.getTitle() : null;
        List<BookmarkId> desktopIds = BookmarkUtils.getDesktopBookmarkIds(bookmarkModel);
        addEmptySpaceOpenOptions(listItems, desktopIds, bookmarkModel, isIncognito, parentTitle);
        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        addCommonActions(
                listItems, /* id= */ null, parentId, isIncognito, /* modifyEnabled= */ false);
        addVisibilityControlActions(listItems, isIncognito);

        return listItems;
    }

    /** Adds options for opening a folder (e.g. bulk "Open all" actions). */
    private void addFolderOpenOptions(
            ModelList listItems,
            BookmarkId folderId,
            BookmarkModel model,
            boolean isIncognito,
            @Nullable String folderTitle) {
        int count = BookmarkUtils.getChildNonFolderBookmarkCountForFolder(model, folderId);
        boolean enabled = count > 0;

        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all,
                                R.plurals.contextmenu_open_all_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openFolderInNewTabs(folderId)));
        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all_in_new_window,
                                R.plurals.contextmenu_open_all_in_new_window_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openFolderInNewWindow(folderId)));
        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all_in_incognito_window,
                                R.plurals.contextmenu_open_all_in_incognito_window_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openFolderInIncognitoWindow(folderId)));
        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all_in_new_tab_group,
                                R.plurals.contextmenu_open_all_in_new_tab_group_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openFolderInNewTabGroup(folderId, folderTitle)));
    }

    /** Adds options for opening a list of bookmarks (e.g. empty space context menu). */
    private void addEmptySpaceOpenOptions(
            ModelList listItems,
            List<BookmarkId> ids,
            BookmarkModel model,
            boolean isIncognito,
            @Nullable String folderTitle) {
        int count = BookmarkUtils.getNonFolderBookmarkCount(model, ids);
        boolean enabled = count > 0;

        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all,
                                R.plurals.contextmenu_open_all_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openBookmarksInNewTabs(ids)));
        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all_in_new_window,
                                R.plurals.contextmenu_open_all_in_new_window_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openBookmarksInNewWindow(ids)));
        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all_in_incognito_window,
                                R.plurals.contextmenu_open_all_in_incognito_window_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openBookmarksInIncognitoWindow(ids)));
        listItems.add(
                buildContextMenuItem(
                        getOpenBookmarkQuantityTitle(
                                R.string.contextmenu_open_all_in_new_tab_group,
                                R.plurals.contextmenu_open_all_in_new_tab_group_plural,
                                count),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openBookmarksInNewTabGroup(ids, folderTitle)));
    }

    private String getOpenBookmarkQuantityTitle(
            @StringRes int stringResId, @PluralsRes int pluralResId, int count) {
        if (count == 0) {
            return mContext.getString(stringResId);
        }
        return mContext.getResources().getQuantityString(pluralResId, count, count);
    }

    /** Adds options for opening a single bookmark item. */
    private void addBookmarkOpenOptions(ModelList listItems, BookmarkId id, boolean isIncognito) {
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_in_new_tab),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openInNewTab(id)));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_in_new_window),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openInNewWindow(id)));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_in_incognito_window),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openInIncognitoWindow(id)));
    }

    private void addModifyOptions(
            ModelList listItems, BookmarkId id, boolean isIncognito, boolean enabled) {
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_edit_bookmark_ellipsis),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> editBookmark(id)));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.bookmark_item_move),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> moveBookmark(id)));

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));

        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.bookmark_item_delete),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> deleteBookmark(id)));
    }

    /**
     * Adds actions common to all bookmarks bar context menus, including folder/bookmark
     * modification options (edit, move, delete) and bar-level settings (add page, add folder, open
     * manager).
     */
    private void addCommonActions(
            ModelList listItems,
            @Nullable BookmarkId id,
            BookmarkId parentId,
            boolean isIncognito,
            boolean modifyEnabled) {
        if (id != null) {
            addModifyOptions(listItems, id, isIncognito, modifyEnabled);
            listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        }
        final Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab != null && currentTab.getUrl() != null && currentTab.getUrl().isValid()) {
            listItems.add(
                    buildContextMenuItem(
                            mContext.getString(R.string.contextmenu_add_page),
                            /* iconResId= */ 0,
                            isIncognito,
                            /* enabled= */ true,
                            v -> addPage(parentId)));
        }

        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_add_folder),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> addFolder(parentId)));

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));

        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_bookmarks_manager),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openBookmarksManager(parentId)));
    }

    /**
     * Adds actions common to all bookmarks bar context menus that are specific to the visibility of
     * the bookmarks bar, which may appear in different ways based on feature flags.
     */
    private void addVisibilityControlActions(ModelList listItems, boolean isIncognito) {
        // When the tri-state feature flag is not enabled, we use the v1 simple toggle.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.BOOKMARKS_BAR_NTP)) {
            listItems.add(
                    buildContextMenuItem(
                            mContext.getString(R.string.contextmenu_show_bookmarks_bar),
                            R.drawable.material_ic_check_24dp,
                            isIncognito,
                            /* enabled= */ true,
                            v -> toggleBookmarksBar()));
            return;
        }

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FLYOUT_IN_BOOKMARKS_BAR)) {
            listItems.add(
                    buildContextMenuSubmenuItem(
                            mContext.getString(R.string.bookmark_bar_settings_title),
                            isIncognito,
                            /* enabled= */ true,
                            () -> buildVisibilityStateMenuItems(isIncognito)));
        } else {
            for (ListItem item : buildVisibilityStateMenuItems(isIncognito)) {
                listItems.add(item);
            }
        }
    }

    private List<ListItem> buildVisibilityStateMenuItems(boolean isIncognito) {
        boolean isXrFullSpaceMode =
                mXrSpaceModeObservableSupplier.get() != null
                        && mXrSpaceModeObservableSupplier.get();
        @BookmarkBarVisibilityState
        int currentState =
                BookmarkBarUtils.getBookmarkBarVisibilityState(
                        mContext, mProfileSupplier.get(), isXrFullSpaceMode);

        List<ListItem> items = new ArrayList<>(3);
        items.add(
                buildCheckableContextMenuItem(
                        mContext.getString(R.string.contextmenu_always_hide_bookmarks_bar),
                        currentState == BookmarkBarVisibilityState.ALWAYS_HIDE,
                        /* position= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> alwaysHide()));
        items.add(
                buildCheckableContextMenuItem(
                        mContext.getString(R.string.contextmenu_always_show_bookmarks_bar),
                        currentState == BookmarkBarVisibilityState.ALWAYS_SHOW,
                        /* position= */ 1,
                        isIncognito,
                        /* enabled= */ true,
                        v -> alwaysShow()));
        items.add(
                buildCheckableContextMenuItem(
                        mContext.getString(R.string.contextmenu_only_show_bookmarks_bar_on_ntp),
                        currentState == BookmarkBarVisibilityState.ONLY_SHOW_ON_NTP,
                        /* position= */ 2,
                        isIncognito,
                        /* enabled= */ true,
                        v -> onlyShowOnNTP()));
        return items;
    }

    private void openInNewTab(BookmarkId id) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB);
        mContextMenuDelegate.openInNewTab(id);
        mDismissRunnable.run();
    }

    private void openInNewWindow(BookmarkId id) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_WINDOW);
        mContextMenuDelegate.openInNewWindow(id);
        mDismissRunnable.run();
    }

    private void openInIncognitoWindow(BookmarkId id) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_INCOGNITO_WINDOW);
        mContextMenuDelegate.openInIncognitoWindow(id);
        mDismissRunnable.run();
    }

    private void editBookmark(BookmarkId id) {
        recordAction(BookmarkBarContextMenuAction.EDIT);
        mContextMenuDelegate.editBookmark(id);
        mDismissRunnable.run();
    }

    private void moveBookmark(BookmarkId id) {
        recordAction(BookmarkBarContextMenuAction.MOVE);
        mContextMenuDelegate.moveBookmark(id);
        mDismissRunnable.run();
    }

    private void deleteBookmark(BookmarkId id) {
        recordAction(BookmarkBarContextMenuAction.DELETE);
        mContextMenuDelegate.deleteBookmark(id);
        mDismissRunnable.run();
    }

    private void openFolderInNewTabs(BookmarkId folderId) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB);
        mContextMenuDelegate.openFolderInNewTabs(folderId);
        mDismissRunnable.run();
    }

    private void openFolderInNewWindow(BookmarkId folderId) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_WINDOW);
        mContextMenuDelegate.openFolderInNewWindow(folderId);
        mDismissRunnable.run();
    }

    private void openFolderInIncognitoWindow(BookmarkId folderId) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_INCOGNITO_WINDOW);
        mContextMenuDelegate.openFolderInIncognitoWindow(folderId);
        mDismissRunnable.run();
    }

    private void openFolderInNewTabGroup(BookmarkId folderId, @Nullable String title) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB_GROUP);
        mContextMenuDelegate.openFolderInNewTabGroup(folderId, title);
        mDismissRunnable.run();
    }

    private void openBookmarksInNewTabs(List<BookmarkId> ids) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB);
        mContextMenuDelegate.openBookmarksInNewTabs(ids);
        mDismissRunnable.run();
    }

    private void openBookmarksInNewWindow(List<BookmarkId> ids) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_WINDOW);
        mContextMenuDelegate.openBookmarksInNewWindow(ids);
        mDismissRunnable.run();
    }

    private void openBookmarksInIncognitoWindow(List<BookmarkId> ids) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_INCOGNITO_WINDOW);
        mContextMenuDelegate.openBookmarksInIncognitoWindow(ids);
        mDismissRunnable.run();
    }

    private void openBookmarksInNewTabGroup(List<BookmarkId> ids, @Nullable String title) {
        recordAction(BookmarkBarContextMenuAction.OPEN_IN_NEW_TAB_GROUP);
        mContextMenuDelegate.openBookmarksInNewTabGroup(ids, title);
        mDismissRunnable.run();
    }

    private void addPage(BookmarkId parentId) {
        recordAction(BookmarkBarContextMenuAction.ADD_PAGE);
        mContextMenuDelegate.addPage(parentId);
        mDismissRunnable.run();
    }

    private void addFolder(BookmarkId parentId) {
        recordAction(BookmarkBarContextMenuAction.ADD_FOLDER);
        mContextMenuDelegate.addFolder(parentId);
        mDismissRunnable.run();
    }

    private void openBookmarksManager(BookmarkId folderId) {
        recordAction(BookmarkBarContextMenuAction.OPEN_BOOKMARKS_MANAGER);
        mContextMenuDelegate.openBookmarksManager(folderId);
        mDismissRunnable.run();
    }

    private void toggleBookmarksBar() {
        mContextMenuDelegate.toggleBookmarksBar();
        mDismissRunnable.run();
    }

    private void alwaysHide() {
        mContextMenuDelegate.setBookmarksBarVisibilityToAlwaysHide();
        mDismissRunnable.run();
    }

    private void alwaysShow() {
        mContextMenuDelegate.setBookmarksBarVisibilityToAlwaysShow();
        mDismissRunnable.run();
    }

    private void onlyShowOnNTP() {
        mContextMenuDelegate.setBookmarksBarVisibilityToOnlyShowOnNTP();
        mDismissRunnable.run();
    }

    private ListItem buildContextMenuItem(
            String title,
            @DrawableRes int iconResId,
            boolean isIncognito,
            boolean enabled,
            View.OnClickListener listener) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.END_ICON_ID, iconResId)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIncognito
                                        ? R.color.default_icon_color_light
                                        : R.color.default_icon_color_secondary_tint_list)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.CLICK_LISTENER, listener);
        if (isIncognito) {
            builder.with(
                    ListMenuItemProperties.TEXT_APPEARANCE_ID,
                    R.style.TextAppearance_TextLarge_Primary_Baseline_Light);
        }
        return new ListItem(ListItemType.MENU_ITEM, builder.build());
    }

    private ListItem buildCheckableContextMenuItem(
            String title,
            boolean isChecked,
            int position,
            boolean isIncognito,
            boolean enabled,
            View.OnClickListener listener) {
        int endIconMarginStartPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.bookmarks_bar_context_menu_end_icon_padding);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.END_ICON_MARGIN_START, endIconMarginStartPx)
                        .with(
                                ListMenuItemProperties.END_ICON_ID,
                                isChecked
                                        ? R.drawable.material_ic_check_24dp
                                        : android.R.color.transparent)
                        .with(ListMenuItemProperties.CHECKABLE, true)
                        .with(ListMenuItemProperties.CHECKED, isChecked)
                        .with(ListMenuItemProperties.POSITION, position)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIncognito
                                        ? R.color.default_icon_color_light
                                        : R.color.default_icon_color_secondary_tint_list)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.CLICK_LISTENER, listener);
        if (isIncognito) {
            builder.with(
                    ListMenuItemProperties.TEXT_APPEARANCE_ID,
                    R.style.TextAppearance_TextLarge_Primary_Baseline_Light);
        }
        return new ListItem(ListItemType.MENU_ITEM, builder.build());
    }

    private ListItem buildContextMenuSubmenuItem(
            String title,
            boolean isIncognito,
            boolean enabled,
            Supplier<List<ListItem>> submenuSupplier) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER, submenuSupplier)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIncognito
                                        ? R.color.default_icon_color_light
                                        : R.color.default_icon_color_secondary_tint_list);
        if (isIncognito) {
            builder.with(
                    ListMenuItemProperties.TEXT_APPEARANCE_ID,
                    R.style.TextAppearance_TextLarge_Primary_Baseline_Light);
        }
        return new ListItem(ListItemType.MENU_ITEM_WITH_SUBMENU, builder.build());
    }

    private void recordAction(@BookmarkBarContextMenuAction int action) {
        BookmarkBarContextMenuMetrics.recordAction(mCurrentEntrypoint, action);
    }
}
