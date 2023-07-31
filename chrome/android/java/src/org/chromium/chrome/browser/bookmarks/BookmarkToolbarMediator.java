// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
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
    private final BookmarkAddNewFolderCoordinator mBookmarkAddNewFolderCoordinator;

    // TODO(crbug.com/1413463): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    private BookmarkId mCurrentFolder;

    BookmarkToolbarMediator(Context context, PropertyModel model,
            DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier,
            SelectionDelegate selectionDelegate, BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener, BookmarkUiPrefs bookmarkUiPrefs,
            BookmarkAddNewFolderCoordinator bookmarkAddNewFolderCoordinator) {
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
        mBookmarkAddNewFolderCoordinator = bookmarkAddNewFolderCoordinator;

        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID,
                    getMenuIdFromSortOrder(mBookmarkUiPrefs.getBookmarkRowSortOrder()));
            final @BookmarkRowDisplayPref int displayPref =
                    mBookmarkUiPrefs.getBookmarkRowDisplayPref();
            mModel.set(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID,
                    displayPref == BookmarkRowDisplayPref.COMPACT ? R.id.compact_view
                                                                  : R.id.visual_view);
        }
        bookmarkDelegateSupplier.onAvailable((bookmarkDelegate) -> {
            mBookmarkDelegate = bookmarkDelegate;
            mModel.set(BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE,
                    this::openParentForCurrentFolder);
            mBookmarkDelegate.addUiObserver(this);
            mBookmarkDelegate.notifyStateChange(this);
        });
    }

    boolean onMenuIdClick(@IdRes int id) {
        // Sorting/viewing submenu needs to be caught, but haven't been implemented yet.
        // TODO(crbug.com/1413463): Handle the new toolbar options.
        if (id == R.id.create_new_folder_menu_id) {
            mBookmarkAddNewFolderCoordinator.show(mCurrentFolder);
            return true;
        } else if (id == R.id.normal_options_submenu) {
            return true;
        } else if (id == R.id.sort_by_newest) {
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_oldest) {
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.CHRONOLOGICAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_last_opened) {
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.RECENTLY_USED);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_alpha) {
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_reverse_alpha) {
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_ALPHABETICAL);
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
            if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
                BookmarkUtils.startEditActivity(mContext, mCurrentFolder);
            } else {
                BookmarkAddEditFolderActivity.startEditFolderActivity(mContext, mCurrentFolder);
            }
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
                if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
                    BookmarkUtils.startFolderPickerActivity(
                            mContext, list.toArray(new BookmarkId[0]));
                } else {
                    BookmarkFolderSelectActivity.startFolderSelectActivity(
                            mContext, list.toArray(new BookmarkId[0]));
                }
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
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            // TODO(https://crbug.com/1439583): Update title and buttons.
        } else {
            mModel.set(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE,
                    mode == BookmarkUiMode.SEARCHING);
        }

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
        mCurrentFolder = folder;
        mModel.set(BookmarkToolbarProperties.CURRENT_FOLDER, mCurrentFolder);

        BookmarkItem folderItem =
                mCurrentFolder == null ? null : mBookmarkModel.getBookmarkById(mCurrentFolder);
        mModel.set(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE, folderItem != null);
        mModel.set(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE,
                folderItem != null && folderItem.isEditable());
        if (folderItem == null) return;

        // Title, navigation buttons.
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
        // This doesn't handle selection state correctly, must be before we fake a selection change.
        mModel.set(BookmarkToolbarProperties.TITLE, title);

        // Selection state isn't routed through MVC, but instead the View directly subscribes to
        // events. The view then changes/ignores/overrides properties that were set above, based on
        // selection. This is problematic because it means the View is sensitive to the order of
        // inputs. To mitigate this, always make it re-apply selection the above properties.
        mModel.set(BookmarkToolbarProperties.FAKE_SELECTION_STATE_CHANGE, true);

        // Should typically be the last thing done, because lots of other properties will trigger
        // an incorrect button state, and we need to override that.
        mModel.set(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE, navigationButton);

        // New folder button.
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mModel.set(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE, true);
            mModel.set(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED,
                    BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, mCurrentFolder));
        }
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

    private @IdRes int getMenuIdFromSortOrder(@BookmarkRowSortOrder int sortOrder) {
        switch (sortOrder) {
            case BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL:
                return R.id.sort_by_newest;
            case BookmarkRowSortOrder.CHRONOLOGICAL:
                return R.id.sort_by_oldest;
            case BookmarkRowSortOrder.ALPHABETICAL:
                return R.id.sort_by_alpha;
            case BookmarkRowSortOrder.REVERSE_ALPHABETICAL:
                return R.id.sort_by_reverse_alpha;
            case BookmarkRowSortOrder.RECENTLY_USED:
                return R.id.sort_by_last_opened;
        }
        return ResourcesCompat.ID_NULL;
    }

    // Private methods.

    private void openParentForCurrentFolder() {
        mBookmarkDelegate.openFolder(
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, mCurrentFolder));
    }
}
