// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.view.View;

import androidx.annotation.DrawableRes;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
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

    /**
     * Constructs the bookmark bar context menu mediator.
     *
     * @param context The context for retrieving resources.
     * @param profileSupplier Used to access the active user profile.
     * @param currentTabSupplier Used to observe or retrieve the active tab.
     * @param contextMenuDelegate Delegate handling context menu actions.
     * @param dismissRunnable Runnable invoked to dismiss the popup menu.
     */
    BookmarkBarContextMenuMediator(
            Context context,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<@Nullable Tab> currentTabSupplier,
            BookmarkBarContextMenuDelegate contextMenuDelegate,
            Runnable dismissRunnable) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mCurrentTabSupplier = currentTabSupplier;
        mContextMenuDelegate = contextMenuDelegate;
        mDismissRunnable = dismissRunnable;
    }

    ModelList buildContextMenuModelList(BookmarkItem item, BookmarkModel bookmarkModel) {
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
            addFolderOpenOptions(
                    listItems,
                    bookmarkModel.getChildIds(id),
                    bookmarkModel,
                    isIncognito,
                    item.getTitle());
        } else {
            addBookmarkOpenOptions(listItems, id, isIncognito);
        }

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        addCommonActions(listItems, id, parentId, isIncognito, canEditOrMoveOrDelete);
        addVisibilityControlActions(listItems, isIncognito);

        return listItems;
    }

    ModelList buildBookmarksBarEmptySpaceContextMenuModelList(BookmarkModel bookmarkModel) {
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
        addFolderOpenOptions(listItems, desktopIds, bookmarkModel, isIncognito, parentTitle);
        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        addCommonActions(
                listItems, /* id= */ null, parentId, isIncognito, /* modifyEnabled= */ false);
        addVisibilityControlActions(listItems, isIncognito);

        return listItems;
    }

    /** Adds options for opening a folder (e.g. bulk "Open all" actions). */
    private void addFolderOpenOptions(
            ModelList listItems,
            List<BookmarkId> childIds,
            BookmarkModel model,
            boolean isIncognito,
            @Nullable String folderTitle) {
        List<BookmarkId> urls = new ArrayList<>();
        for (BookmarkId id : childIds) {
            BookmarkItem child = model.getBookmarkById(id);
            if (child != null && !child.isFolder()) urls.add(id);
        }
        int count = urls.size();
        String openAllText;
        String openAllNewWindowText;
        String openAllIncognitoText;
        String openAllTabGroupText;

        if (count == 0) {
            openAllText = mContext.getString(R.string.contextmenu_open_all);
            openAllNewWindowText = mContext.getString(R.string.contextmenu_open_all_in_new_window);
            openAllIncognitoText =
                    mContext.getString(R.string.contextmenu_open_all_in_incognito_window);
            openAllTabGroupText =
                    mContext.getString(R.string.contextmenu_open_all_in_new_tab_group);
        } else {
            openAllText =
                    mContext.getResources()
                            .getQuantityString(R.plurals.contextmenu_open_all_plural, count, count);
            openAllNewWindowText =
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.contextmenu_open_all_in_new_window_plural,
                                    count,
                                    count);
            openAllIncognitoText =
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.contextmenu_open_all_in_incognito_window_plural,
                                    count,
                                    count);
            openAllTabGroupText =
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.contextmenu_open_all_in_new_tab_group_plural,
                                    count,
                                    count);
        }

        boolean enabled = count > 0;
        listItems.add(
                buildContextMenuItem(
                        openAllText, /* iconResId= */ 0, isIncognito, enabled, v -> openAll(urls)));
        listItems.add(
                buildContextMenuItem(
                        openAllNewWindowText,
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openAllInNewWindow(urls)));
        listItems.add(
                buildContextMenuItem(
                        openAllIncognitoText,
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openAllInIncognitoWindow(urls)));
        listItems.add(
                buildContextMenuItem(
                        openAllTabGroupText,
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openAllInNewTabGroup(urls, folderTitle)));
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

        // If the tri-state feature flag is enabled, we will use multiple options.
        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_always_hide_bookmarks_bar),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> toggleBookmarksBar()));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_always_show_bookmarks_bar),
                        R.drawable.material_ic_check_24dp,
                        isIncognito,
                        /* enabled= */ true,
                        v -> toggleBookmarksBar()));
    }

    private void openInNewTab(BookmarkId id) {
        mContextMenuDelegate.openInNewTab(id);
        mDismissRunnable.run();
    }

    private void openInNewWindow(BookmarkId id) {
        mContextMenuDelegate.openInNewWindow(id);
        mDismissRunnable.run();
    }

    private void openInIncognitoWindow(BookmarkId id) {
        mContextMenuDelegate.openInIncognitoWindow(id);
        mDismissRunnable.run();
    }

    private void editBookmark(BookmarkId id) {
        mContextMenuDelegate.editBookmark(id);
        mDismissRunnable.run();
    }

    private void moveBookmark(BookmarkId id) {
        mContextMenuDelegate.moveBookmark(id);
        mDismissRunnable.run();
    }

    private void deleteBookmark(BookmarkId id) {
        mContextMenuDelegate.deleteBookmark(id);
        mDismissRunnable.run();
    }

    private void openAll(List<BookmarkId> ids) {
        mContextMenuDelegate.openAll(ids);
        mDismissRunnable.run();
    }

    private void openAllInNewWindow(List<BookmarkId> ids) {
        mContextMenuDelegate.openAllInNewWindow(ids);
        mDismissRunnable.run();
    }

    private void openAllInIncognitoWindow(List<BookmarkId> ids) {
        mContextMenuDelegate.openAllInIncognitoWindow(ids);
        mDismissRunnable.run();
    }

    private void openAllInNewTabGroup(List<BookmarkId> ids, @Nullable String title) {
        mContextMenuDelegate.openAllInNewTabGroup(ids, title);
        mDismissRunnable.run();
    }

    private void addPage(BookmarkId parentId) {
        mContextMenuDelegate.addPage(parentId);
        mDismissRunnable.run();
    }

    private void addFolder(BookmarkId parentId) {
        mContextMenuDelegate.addFolder(parentId);
        mDismissRunnable.run();
    }

    private void openBookmarksManager(BookmarkId folderId) {
        mContextMenuDelegate.openBookmarksManager(folderId);
        mDismissRunnable.run();
    }

    private void toggleBookmarksBar() {
        mContextMenuDelegate.toggleBookmarksBar();
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
}
