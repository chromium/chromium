// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.R;
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

/** Handles the business logic and stubs for the Bookmarks Bar context menu. */
@NullMarked
class BookmarkBarContextMenuMediator {
    private final Context mContext;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Supplier<@Nullable Tab> mCurrentTabSupplier;
    private final Runnable mDismissRunnable;

    BookmarkBarContextMenuMediator(
            Context context,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<@Nullable Tab> currentTabSupplier,
            Runnable dismissRunnable) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mCurrentTabSupplier = currentTabSupplier;
        mDismissRunnable = dismissRunnable;
    }

    ModelList buildContextMenuModelList(BookmarkItem item, BookmarkModel bookmarkModel) {
        final Profile profile = mProfileSupplier.get();
        if (profile == null) return new ModelList();

        final boolean isIncognito = profile.isOffTheRecord();
        final boolean isBookmarksBarFolder =
                item.getId().equals(bookmarkModel.getDesktopFolderId())
                        || (bookmarkModel.getAccountDesktopFolderId() != null
                                && item.getId().equals(bookmarkModel.getAccountDesktopFolderId()));
        boolean canEditOrMoveOrDelete = !isBookmarksBarFolder;

        ModelList listItems = new ModelList();

        if (item.isFolder()) {
            addFolderOpenOptions(
                    listItems, bookmarkModel.getChildIds(item.getId()), bookmarkModel, isIncognito);
        } else {
            addBookmarkOpenOptions(listItems, isIncognito);
        }

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        addCommonActions(listItems, isIncognito, canEditOrMoveOrDelete);

        return listItems;
    }

    ModelList buildBookmarksBarEmptySpaceContextMenuModelList(BookmarkModel bookmarkModel) {
        final Profile profile = mProfileSupplier.get();
        if (profile == null) return new ModelList();

        final boolean isIncognito = profile.isOffTheRecord();
        ModelList listItems = new ModelList();

        List<BookmarkId> desktopIds = BookmarkUtils.getDesktopBookmarkIds(bookmarkModel);
        addFolderOpenOptions(listItems, desktopIds, bookmarkModel, isIncognito);
        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        addCommonActions(listItems, isIncognito, /* modifyEnabled= */ false);

        return listItems;
    }

    /** Adds options for opening a folder (e.g. bulk "Open all" actions). */
    private void addFolderOpenOptions(
            ModelList listItems,
            List<BookmarkId> childIds,
            BookmarkModel model,
            boolean isIncognito) {
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
                        openAllText, /* iconResId= */ 0, isIncognito, enabled, v -> openAll()));
        listItems.add(
                buildContextMenuItem(
                        openAllNewWindowText,
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openAllInNewWindow()));
        listItems.add(
                buildContextMenuItem(
                        openAllIncognitoText,
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openAllInIncognitoWindow()));
        listItems.add(
                buildContextMenuItem(
                        openAllTabGroupText,
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> openAllInNewTabGroup()));
    }

    /** Adds options for opening a single bookmark item. */
    private void addBookmarkOpenOptions(ModelList listItems, boolean isIncognito) {
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_in_new_tab),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openInNewTab()));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_in_new_window),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openInNewWindow()));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_in_incognito_window),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openInIncognitoWindow()));
    }

    private void addModifyOptions(ModelList listItems, boolean isIncognito, boolean enabled) {
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_edit_bookmark_ellipsis),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> editBookmark()));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.bookmark_item_move),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> moveBookmark()));

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));

        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.bookmark_item_delete),
                        /* iconResId= */ 0,
                        isIncognito,
                        enabled,
                        v -> deleteBookmark()));
    }

    /**
     * Adds actions common to all bookmarks bar context menus, including folder/bookmark
     * modification options (edit, move, delete) and bar-level settings (add page, add folder, open
     * manager, show bar).
     */
    private void addCommonActions(ModelList listItems, boolean isIncognito, boolean modifyEnabled) {
        addModifyOptions(listItems, isIncognito, modifyEnabled);
        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));
        final Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab != null
                && currentTab.getOriginalUrl() != null
                && currentTab.getOriginalUrl().isValid()) {
            listItems.add(
                    buildContextMenuItem(
                            mContext.getString(R.string.contextmenu_add_page),
                            /* iconResId= */ 0,
                            isIncognito,
                            /* enabled= */ true,
                            v -> addPage()));
        }

        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_add_folder),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> addFolder()));

        listItems.add(BasicListMenu.buildMenuDivider(isIncognito));

        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_open_bookmarks_manager),
                        /* iconResId= */ 0,
                        isIncognito,
                        /* enabled= */ true,
                        v -> openBookmarksManager()));
        listItems.add(
                buildContextMenuItem(
                        mContext.getString(R.string.contextmenu_show_bookmarks_bar),
                        R.drawable.material_ic_check_24dp,
                        isIncognito,
                        /* enabled= */ true,
                        v -> toggleBookmarksBar()));
    }

    private void addPage() {
        // TODO(crbug.com/465996578): Add page.
        mDismissRunnable.run();
    }

    private void addFolder() {
        // TODO(crbug.com/465996578): Add folder.
        mDismissRunnable.run();
    }

    private void openBookmarksManager() {
        // TODO(crbug.com/465996578): Open bookmarks manager.
        mDismissRunnable.run();
    }

    private void toggleBookmarksBar() {
        // TODO(crbug.com/465996578): Show bookmarks bar.
        mDismissRunnable.run();
    }

    private void openInNewTab() {
        // TODO(crbug.com/465996578): Open in new tab.
        mDismissRunnable.run();
    }

    private void openInNewWindow() {
        // TODO(crbug.com/465996578): Open in new window.
        mDismissRunnable.run();
    }

    private void openInIncognitoWindow() {
        // TODO(crbug.com/465996578): Open in incognito window.
        mDismissRunnable.run();
    }

    private void editBookmark() {
        // TODO(crbug.com/465996578): Edit folder/bookmark.
        mDismissRunnable.run();
    }

    private void moveBookmark() {
        // TODO(crbug.com/465996578): Move folder/bookmark.
        mDismissRunnable.run();
    }

    private void deleteBookmark() {
        // TODO(crbug.com/465996578): Delete folder/bookmark.
        mDismissRunnable.run();
    }

    private void openAll() {
        // TODO(crbug.com/465996578): Open all.
        mDismissRunnable.run();
    }

    private void openAllInNewWindow() {
        // TODO(crbug.com/465996578): Open all in new window.
        mDismissRunnable.run();
    }

    private void openAllInIncognitoWindow() {
        // TODO(crbug.com/465996578): Open all in incognito window.
        mDismissRunnable.run();
    }

    private void openAllInNewTabGroup() {
        // TODO(crbug.com/465996578): Open all in new tab group.
        mDismissRunnable.run();
    }

    private ListItem buildContextMenuItem(
            String title,
            @DrawableRes int iconResId,
            boolean isIncognito,
            boolean enabled,
            View.OnClickListener listener) {
        Drawable icon = iconResId != 0 ? AppCompatResources.getDrawable(mContext, iconResId) : null;
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, icon)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIncognito
                                        ? R.color.default_icon_color_light
                                        : R.color.default_icon_color_secondary_tint_list)
                        .with(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN, true)
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
