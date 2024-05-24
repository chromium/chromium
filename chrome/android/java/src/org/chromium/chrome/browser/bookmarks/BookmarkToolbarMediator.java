// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.Observer;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.function.BooleanSupplier;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
class BookmarkToolbarMediator
        implements BookmarkUiObserver,
                DragListener,
                SelectionDelegate.SelectionObserver<BookmarkId> {
    @VisibleForTesting
    static final List<Integer> SORT_MENU_IDS =
            Arrays.asList(
                    R.id.sort_by_manual,
                    R.id.sort_by_newest,
                    R.id.sort_by_oldest,
                    R.id.sort_by_last_opened,
                    R.id.sort_by_alpha,
                    R.id.sort_by_reverse_alpha);

    private final BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver =
            new Observer() {
                @Override
                public void onBookmarkRowDisplayPrefChanged(
                        @BookmarkRowDisplayPref int displayPref) {
                    mModel.set(
                            BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID,
                            getMenuIdFromDisplayPref(displayPref));
                }

                @Override
                public void onBookmarkRowSortOrderChanged(@BookmarkRowSortOrder int sortOrder) {
                    mModel.set(
                            BookmarkToolbarProperties.CHECKED_SORT_MENU_ID,
                            getMenuIdFromSortOrder(sortOrder));
                }
            };

    private final Context mContext;
    private final PropertyModel mModel;
    private final DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    private final SelectionDelegate mSelectionDelegate;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkOpener mBookmarkOpener;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final BookmarkAddNewFolderCoordinator mBookmarkAddNewFolderCoordinator;
    private final Runnable mEndSearchRunnable;
    private final BookmarkMoveSnackbarManager mBookmarkMoveSnackbarManager;
    private final BooleanSupplier mIncognitoEnabledSupplier;

    // TODO(crbug.com/40255666): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    private BookmarkId mCurrentFolder;
    private @BookmarkUiMode int mCurrentUiMode;

    BookmarkToolbarMediator(
            Context context,
            PropertyModel model,
            DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier,
            SelectionDelegate<BookmarkId> selectionDelegate,
            BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener,
            BookmarkUiPrefs bookmarkUiPrefs,
            BookmarkAddNewFolderCoordinator bookmarkAddNewFolderCoordinator,
            Runnable endSearchRunnable,
            BookmarkMoveSnackbarManager bookmarkMoveSnackbarManager,
            BooleanSupplier incognitoEnabledSupplier) {
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
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);
        mBookmarkAddNewFolderCoordinator = bookmarkAddNewFolderCoordinator;
        mEndSearchRunnable = endSearchRunnable;
        mBookmarkMoveSnackbarManager = bookmarkMoveSnackbarManager;
        mIncognitoEnabledSupplier = incognitoEnabledSupplier;

        mModel.set(BookmarkToolbarProperties.SORT_MENU_IDS, SORT_MENU_IDS);
        mModel.set(
                BookmarkToolbarProperties.CHECKED_SORT_MENU_ID,
                getMenuIdFromSortOrder(mBookmarkUiPrefs.getBookmarkRowSortOrder()));
        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();
        mModel.set(
                BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID,
                displayPref == BookmarkRowDisplayPref.COMPACT
                        ? R.id.compact_view
                        : R.id.visual_view);

        bookmarkDelegateSupplier.onAvailable(
                (bookmarkDelegate) -> {
                    mBookmarkDelegate = bookmarkDelegate;
                    mModel.set(
                            BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE, this::onNavigateBack);
                    mBookmarkDelegate.addUiObserver(this);
                    mBookmarkDelegate.notifyStateChange(this);
                });
    }

    boolean onMenuIdClick(@IdRes int id) {
        // Sorting/viewing submenu needs to be caught, but haven't been implemented yet.
        if (id == R.id.create_new_folder_menu_id) {
            mBookmarkAddNewFolderCoordinator.show(mCurrentFolder);
            return true;
        } else if (id == R.id.normal_options_submenu) {
            return true;
        } else if (id == R.id.sort_by_manual) {
            assert SORT_MENU_IDS.contains(id);
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.MANUAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_newest) {
            assert SORT_MENU_IDS.contains(id);
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_oldest) {
            assert SORT_MENU_IDS.contains(id);
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.CHRONOLOGICAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_last_opened) {
            assert SORT_MENU_IDS.contains(id);
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.RECENTLY_USED);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_alpha) {
            assert SORT_MENU_IDS.contains(id);
            mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);
            mModel.set(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID, id);
            return true;
        } else if (id == R.id.sort_by_reverse_alpha) {
            assert SORT_MENU_IDS.contains(id);
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
            BookmarkUtils.startEditActivity(mContext, mCurrentFolder);
            return true;
        } else if (id == R.id.close_menu_id) {
            BookmarkUtils.finishActivityOnPhone(mContext);
            return true;
        } else if (id == R.id.selection_mode_edit_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            assert list.size() == 1;
            BookmarkItem item = mBookmarkModel.getBookmarkById(list.get(0));
            BookmarkUtils.startEditActivity(mContext, item.getId());
            return true;
        } else if (id == R.id.selection_mode_move_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                mBookmarkMoveSnackbarManager.startFolderPickerAndObserveResult(
                        list.toArray(new BookmarkId[0]));
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
                    mSelectionDelegate.getSelectedItemsAsList(), /* incognito= */ false);
            return true;
        } else if (id == R.id.selection_open_in_incognito_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInIncognito");
            RecordHistogram.recordCount1000Histogram(
                    "Bookmarks.Count.OpenInIncognito",
                    mSelectionDelegate.getSelectedItems().size());
            mBookmarkOpener.openBookmarksInNewTabs(
                    mSelectionDelegate.getSelectedItemsAsList(), /* incognito= */ true);
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
                mBookmarkModel.setReadStatusForReadingList(
                        bookmarkItem.getId(), /* read= */ id == R.id.reading_list_mark_as_read_id);
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
        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);

        if (mBookmarkDelegate != null) {
            mBookmarkDelegate.removeUiObserver(this);
        }
    }

    @Override
    public void onUiModeChanged(@BookmarkUiMode int mode) {
        mCurrentUiMode = mode;

        // TODO(https://crbug.com/1439583): Update buttons.
        mModel.set(BookmarkToolbarProperties.BOOKMARK_UI_MODE, mode);
        if (mode == BookmarkUiMode.LOADING) {
            mModel.set(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE, NavigationButton.NONE);
            mModel.set(BookmarkToolbarProperties.TITLE, null);
            mModel.set(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE, false);
        } else if (mode == BookmarkUiMode.SEARCHING) {
            mModel.set(
                    BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE,
                    NavigationButton.NORMAL_VIEW_BACK);
            if (!mSelectionDelegate.isSelectionEnabled()) {
                mModel.set(
                        BookmarkToolbarProperties.TITLE,
                        mContext.getString(R.string.bookmark_toolbar_search_title));
            }
            mModel.set(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE, false);
            mModel.set(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED, false);
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

        BookmarkItem folderItem =
                mCurrentFolder == null ? null : mBookmarkModel.getBookmarkById(mCurrentFolder);
        mModel.set(
                BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE,
                folderItem != null && folderItem.isEditable());
        if (folderItem == null) return;

        // Title, navigation buttons.
        String title;
        @NavigationButton int navigationButton;
        Resources res = mContext.getResources();
        if (folder.equals(mBookmarkModel.getRootFolderId())) {
            title = res.getString(R.string.bookmarks);
            navigationButton = NavigationButton.NONE;
        } else if (mBookmarkModel.getTopLevelFolderIds().contains(folderItem.getParentId())
                && TextUtils.isEmpty(folderItem.getTitle())) {
            title = res.getString(R.string.bookmarks);
            navigationButton = NavigationButton.NORMAL_VIEW_BACK;
        } else {
            title = folderItem.getTitle();
            navigationButton = NavigationButton.NORMAL_VIEW_BACK;
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
        mModel.set(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE, true);
        mModel.set(
                BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED,
                BookmarkUtils.canAddFolderToParent(mBookmarkModel, mCurrentFolder));

        // Special behavior in reading list:
        // - Select CHRONOLOGICAL as sort order.
        // - Disable sort menu items.
        boolean inReadingList = BookmarkUtils.isReadingListFolder(mBookmarkModel, mCurrentFolder);
        mModel.set(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED, !inReadingList);
        if (inReadingList) {
            // Reading list items are always sorted by date added.
            mModel.set(
                    BookmarkToolbarProperties.CHECKED_SORT_MENU_ID,
                    getMenuIdFromSortOrder(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL));
        } else {
            mModel.set(
                    BookmarkToolbarProperties.CHECKED_SORT_MENU_ID,
                    getMenuIdFromSortOrder(mBookmarkUiPrefs.getBookmarkRowSortOrder()));
        }
    }

    // DragReorderableListAdapter.DragListener implementation.

    @Override
    public void onDragStateChange(boolean dragEnabled) {
        mModel.set(BookmarkToolbarProperties.DRAG_ENABLED, dragEnabled);
    }

    // SelectionDelegate.SelectionObserver implementation.

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedItems) {
        if (!mSelectionDelegate.isSelectionEnabled()) {
            onUiModeChanged(mCurrentUiMode);

            assert selectedItems.isEmpty();
        }
        updateSelectedMenuItemVisibility(selectedItems);

        mModel.set(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE, false);
    }

    private void updateSelectedMenuItemVisibility(List<BookmarkId> selectedBookmarks) {
        boolean showEdit = selectedBookmarks.size() == 1;
        boolean showOpenInNewTab = selectedBookmarks.size() > 0;
        boolean showOpenInIncognito =
                selectedBookmarks.size() > 0 && mIncognitoEnabledSupplier.getAsBoolean();
        boolean showMove = selectedBookmarks.size() > 0;
        boolean showMarkRead;
        boolean showMarkUnread;

        // It does not make sense to open a folder in new tab.
        for (BookmarkId bookmark : selectedBookmarks) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(bookmark);
            if (item != null && item.isFolder()) {
                showOpenInNewTab = false;
                showOpenInIncognito = false;
                break;
            }
        }

        boolean hasPartnerBoomarkSelected = false;
        // Partner bookmarks can't move, so if the selection includes a partner bookmark,
        // disable the move button.
        for (BookmarkId bookmark : selectedBookmarks) {
            if (bookmark.getType() == BookmarkType.PARTNER) {
                hasPartnerBoomarkSelected = true;
                showMove = false;
                break;
            }
        }
        if (hasPartnerBoomarkSelected) {
            showMove = false;
            showEdit = false;
        }

        // Compute whether all selected bookmarks are reading list items and add up the number
        // of read items.
        int numReadingListItems = 0;
        int numRead = 0;
        for (int i = 0; i < selectedBookmarks.size(); i++) {
            BookmarkId bookmark = selectedBookmarks.get(i);
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmark);
            if (bookmark.getType() == BookmarkType.READING_LIST) {
                numReadingListItems++;
                if (bookmarkItem.isRead()) numRead++;
            }
        }

        // Only show the "mark as" options when all selections are reading list items and
        // have the same read state.
        boolean onlyReadingListSelected =
                selectedBookmarks.size() > 0 && numReadingListItems == selectedBookmarks.size();
        showMarkRead = onlyReadingListSelected && numRead == 0;
        showMarkUnread = onlyReadingListSelected && numRead == selectedBookmarks.size();

        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT, showEdit);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB, showOpenInNewTab);
        mModel.set(
                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                showOpenInIncognito);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE, showMove);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ, showMarkRead);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD, showMarkUnread);
    }

    private @IdRes int getMenuIdFromDisplayPref(@BookmarkRowDisplayPref int displayPref) {
        switch (displayPref) {
            case BookmarkRowDisplayPref.COMPACT:
                return R.id.compact_view;
            case BookmarkRowDisplayPref.VISUAL:
                return R.id.visual_view;
        }
        return ResourcesCompat.ID_NULL;
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
            case BookmarkRowSortOrder.MANUAL:
                return R.id.sort_by_manual;
        }
        return ResourcesCompat.ID_NULL;
    }

    // Private methods.

    private void onNavigateBack() {
        if (mCurrentUiMode == BookmarkUiMode.SEARCHING) {
            mEndSearchRunnable.run();
            return;
        }

        mBookmarkDelegate.openFolder(mBookmarkModel.getBookmarkById(mCurrentFolder).getParentId());
    }
}
