// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
class BookmarkToolbarMediator implements BookmarkUiObserver, DragListener,
                                         SelectionDelegate.SelectionObserver<BookmarkItem> {
    private final Context mContext;
    private final PropertyModel mModel;
    private final DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    private final SelectionDelegate mSelectionDelegate;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkOpener mBookmarkOpener;
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    // TODO(crbug.com/1413463): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    private BookmarkId mCurrentFolder;

    BookmarkToolbarMediator(Context context, PropertyModel model,
            DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier,
            SelectionDelegate selectionDelegate, BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener, BookmarkUiPrefs bookmarkUiPrefs) {
        mContext = context;
        mModel = model;

        mModel.set(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION, this::onMenuIdClick);
        mDragReorderableRecyclerViewAdapter = dragReorderableRecyclerViewAdapter;
        mDragReorderableRecyclerViewAdapter.addDragListener(this);
        mSelectionDelegate = selectionDelegate;
        mSelectionDelegate.addObserver(this);
        mBookmarkModel = bookmarkModel;
        mBookmarkOpener = bookmarkOpener;
        mBookmarkUiPrefs = bookmarkUiPrefs;

        final @BookmarkRowDisplayPref int pref = mBookmarkUiPrefs.getBookmarkRowDisplayPref();
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID,
                    pref == BookmarkRowDisplayPref.COMPACT ? R.id.compact_view : R.id.visual_view);
        }
        bookmarkDelegateSupplier.onAvailable((bookmarkDelegate) -> {
            mBookmarkDelegate = bookmarkDelegate;
            mModel.set(BookmarkToolbarProperties.OPEN_SEARCH_UI_RUNNABLE,
                    mBookmarkDelegate::openSearchUi);
            mModel.set(
                    BookmarkToolbarProperties.OPEN_FOLDER_CALLBACK, mBookmarkDelegate::openFolder);
            mBookmarkDelegate.addUiObserver(this);
            mBookmarkDelegate.notifyStateChange(this);
        });
    }

    boolean onMenuIdClick(@IdRes int id) {
        // Sorting/viewing submenu needs to be caught, but haven't been implemented yet.
        // TODO(crbug.com/1413463): Handle the new toolbar options.
        if (id == R.id.create_new_folder_menu_id) {
            return true;
        } else if (id == R.id.normal_options_submenu) {
            return true;
        } else if (id == R.id.sort_by_newest) {
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_oldest) {
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_alpha) {
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_reverse_alpha) {
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.visual_view) {
            mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID, id);
            return true;
        } else if (id == R.id.compact_view) {
            mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
            mModel.set(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID, id);
            return true;
        } else if (id == R.id.edit_menu_id) {
            BookmarkAddEditFolderActivity.startEditFolderActivity(mContext, mCurrentFolder);
            return true;
        } else if (id == R.id.close_menu_id) {
            BookmarkUtils.finishActivityOnPhone(mContext);
            return true;
        } else if (id == R.id.search_menu_id) {
            assert mBookmarkDelegate != null;
            mBookmarkDelegate.openSearchUi();
            mModel.set(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE, NavigationButton.BACK);
            return true;
        } else if (id == R.id.selection_mode_edit_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            assert list.size() == 1;
            BookmarkItem item = mBookmarkModel.getBookmarkById(list.get(0));
            if (item.isFolder()) {
                BookmarkAddEditFolderActivity.startEditFolderActivity(mContext, item.getId());
            } else {
                BookmarkUtils.startEditActivity(mContext, item.getId());
            }
            return true;
        } else if (id == R.id.selection_mode_move_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                BookmarkFolderSelectActivity.startFolderSelectActivity(
                        mContext, list.toArray(new BookmarkId[0]));
                RecordUserAction.record("MobileBookmarkManagerMoveToFolderBulk");
            }
            return true;
        } else if (id == R.id.selection_mode_delete_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                mBookmarkModel.deleteBookmarks(list.toArray(new BookmarkId[0]));
                RecordUserAction.record("MobileBookmarkManagerDeleteBulk");
            }
            return true;
        } else if (id == R.id.selection_open_in_new_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInNewTab");
            RecordHistogram.recordCount1000Histogram(
                    "Bookmarks.Count.OpenInNewTab", mSelectionDelegate.getSelectedItems().size());
            mBookmarkOpener.openBookmarksInNewTabs(
                    mSelectionDelegate.getSelectedItemsAsList(), /*incognito=*/false);
            return true;
        } else if (id == R.id.selection_open_in_incognito_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInIncognito");
            RecordHistogram.recordCount1000Histogram("Bookmarks.Count.OpenInIncognito",
                    mSelectionDelegate.getSelectedItems().size());
            mBookmarkOpener.openBookmarksInNewTabs(
                    mSelectionDelegate.getSelectedItemsAsList(), /*incognito=*/true);
            return true;
        } else if (id == R.id.reading_list_mark_as_read_id
                || id == R.id.reading_list_mark_as_unread_id) {
            // Handle the seclection "mark as" buttons in the same block because the behavior is
            // the same other than one boolean flip.
            for (int i = 0; i < mSelectionDelegate.getSelectedItemsAsList().size(); i++) {
                BookmarkId bookmark =
                        (BookmarkId) mSelectionDelegate.getSelectedItemsAsList().get(i);
                if (bookmark.getType() != BookmarkType.READING_LIST) continue;

                BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmark);
                mBookmarkModel.setReadStatusForReadingList(bookmarkItem.getUrl(),
                        /*read=*/id == R.id.reading_list_mark_as_read_id);
            }
            mSelectionDelegate.clearSelection();
            return true;
        }

        assert false : "Unhandled menu click.";
        return false;
    }

    // BookmarkUiObserver implementation.

    @Override
    public void onDestroy() {
        mDragReorderableRecyclerViewAdapter.removeDragListener(this);
        mSelectionDelegate.removeObserver(this);

        if (mBookmarkDelegate != null) {
            mBookmarkDelegate.removeUiObserver(this);
        }
    }

    @Override
    public void onUiModeChanged(@BookmarkUiMode int mode) {
        mModel.set(
                BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE, mode == BookmarkUiMode.SEARCHING);
        mModel.set(BookmarkToolbarProperties.BOOKMARK_UI_MODE, mode);
        if (mode == BookmarkUiMode.LOADING) {
            mModel.set(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE, NavigationButton.NONE);
            mModel.set(BookmarkToolbarProperties.TITLE, null);
            mModel.set(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE, false);
            mModel.set(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE, false);
        } else {
            // All modes besides LOADING require a folder to be set. If there's none available,
            // then the button visibilities will be updated accordingly. Additionally, it's
            // possible that the folder was renamed, so refresh the folder UI just in case.
            onFolderStateSet(mCurrentFolder);
        }
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        // If we're in the middle of a selection, do not override things.
        // TODO(https://crbug.com/1435024): Rework logic to not be more robust.
        if (mSelectionDelegate.isSelectionEnabled()) {
            return;
        }

        mCurrentFolder = folder;
        mModel.set(BookmarkToolbarProperties.CURRENT_FOLDER, mCurrentFolder);

        BookmarkItem folderItem =
                mCurrentFolder == null ? null : mBookmarkModel.getBookmarkById(mCurrentFolder);
        mModel.set(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE, folderItem != null);
        mModel.set(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE,
                folderItem != null && folderItem.isEditable());
        if (folderItem == null) return;

        String title;
        @NavigationButton
        int navigationButton;
        Resources res = mContext.getResources();
        if (folder.equals(mBookmarkModel.getRootFolderId())) {
            title = res.getString(R.string.bookmarks);
            navigationButton = NavigationButton.NONE;
        } else if (folder.equals(BookmarkId.SHOPPING_FOLDER)) {
            title = res.getString(R.string.price_tracking_bookmarks_filter_title);
            navigationButton = NavigationButton.BACK;
        } else if (mBookmarkModel.getTopLevelFolderParentIds().contains(folderItem.getParentId())
                && TextUtils.isEmpty(folderItem.getTitle())) {
            title = res.getString(R.string.bookmarks);
            navigationButton = NavigationButton.BACK;
        } else {
            title = folderItem.getTitle();
            navigationButton = NavigationButton.BACK;
        }

        mModel.set(BookmarkToolbarProperties.TITLE, title);
        mModel.set(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE, navigationButton);
    }

    @Override
    public void onBookmarkItemMenuOpened() {
        mModel.set(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE, false);
    }

    // DragReorderableListAdapter.DragListener implementation.

    @Override
    public void onDragStateChange(boolean dragEnabled) {
        mModel.set(BookmarkToolbarProperties.DRAG_ENABLED, dragEnabled);
    }

    // SelectionDelegate.SelectionObserver implementation.

    @Override
    public void onSelectionStateChange(List<BookmarkItem> selectedItems) {
        mModel.set(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE, false);
        if (!mSelectionDelegate.isSelectionEnabled()) {
            onFolderStateSet(mCurrentFolder);
        }
    }
}
