// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.Observer;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DragListener;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;
import java.util.function.BooleanSupplier;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
@NullMarked
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
    private final Profile mProfile;
    private final PropertyModel mModel;
    private final DragTouchHandler mDragTouchHandler;
    private final SelectionDelegate mSelectionDelegate;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkOpener mBookmarkOpener;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final BookmarkAddNewFolderCoordinator mBookmarkAddNewFolderCoordinator;
    private final Runnable mEndSearchRunnable;
    private final BooleanSupplier mIncognitoEnabledSupplier;
    private final BookmarkManagerOpener mBookmarkManagerOpener;
    private final SnackbarManager mSnackbarManager;
    private final Clipboard mClipboard;

    // TODO(crbug.com/40255666): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    private @Nullable BookmarkId mCurrentFolder;
    private @BookmarkUiMode int mCurrentUiMode;

    BookmarkToolbarMediator(
            Context context,
            Profile profile,
            PropertyModel model,
            DragTouchHandler dragTouchHandler,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier,
            SelectionDelegate<BookmarkId> selectionDelegate,
            BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener,
            BookmarkUiPrefs bookmarkUiPrefs,
            BookmarkAddNewFolderCoordinator bookmarkAddNewFolderCoordinator,
            Runnable endSearchRunnable,
            BooleanSupplier incognitoEnabledSupplier,
            BookmarkManagerOpener bookmarkManagerOpener,
            SnackbarManager snackbarManager,
            Clipboard clipboard) {
        mContext = context;
        mProfile = profile;
        mModel = model;

        mModel.set(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION, this::onMenuIdClick);
        mDragTouchHandler = dragTouchHandler;
        mDragTouchHandler.addDragListener(this);
        mSelectionDelegate = selectionDelegate;
        mSelectionDelegate.addObserver(this);
        mBookmarkModel = bookmarkModel;
        mBookmarkOpener = bookmarkOpener;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);
        mBookmarkAddNewFolderCoordinator = bookmarkAddNewFolderCoordinator;
        mEndSearchRunnable = endSearchRunnable;
        mIncognitoEnabledSupplier = incognitoEnabledSupplier;
        mBookmarkManagerOpener = bookmarkManagerOpener;
        mSnackbarManager = snackbarManager;
        mClipboard = clipboard;

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
            mBookmarkAddNewFolderCoordinator.show(assumeNonNull(mCurrentFolder));
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
            mBookmarkManagerOpener.startEditActivity(
                    mContext, mProfile, assumeNonNull(mCurrentFolder));
            return true;
        } else if (id == R.id.close_menu_id) {
            mBookmarkManagerOpener.finishActivityOnPhone(mContext);
            return true;
        } else if (id == R.id.selection_mode_edit_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            assert list.size() == 1;
            BookmarkItem item = mBookmarkModel.getBookmarkById(list.get(0));
            assumeNonNull(item);
            mBookmarkManagerOpener.startEditActivity(mContext, mProfile, item.getId());
            return true;
        } else if (id == R.id.selection_mode_move_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                mBookmarkManagerOpener.startFolderPickerActivity(
                        mContext, mProfile, list.toArray(new BookmarkId[0]));
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
        } else if (id == R.id.selection_mode_copy_link) {
            RecordUserAction.record("MobileBookmarkManagerCopyLink");
            List<BookmarkId> selectedItems = mSelectionDelegate.getSelectedItemsAsList();
            assert selectedItems.size() == 1;
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(selectedItems.get(0));
            assumeNonNull(bookmarkItem);
            mClipboard.setText(bookmarkItem.getUrl().getSpec());
            mSelectionDelegate.clearSelection();
            Snackbar snackbar =
                    Snackbar.make(
                            mContext.getString(R.string.copied),
                            null,
                            Snackbar.TYPE_NOTIFICATION,
                            Snackbar.UMA_BOOKMARK_LINK_COPIED);
            mSnackbarManager.showSnackbar(snackbar);
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
                assumeNonNull(bookmarkItem);
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
        mDragTouchHandler.removeDragListener(this);
        mSelectionDelegate.removeObserver(this);
        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);

        if (mBookmarkDelegate != null) {
            mBookmarkDelegate.removeUiObserver(this);
        }
    }

    @Override
    public void onUiModeChanged(@BookmarkUiMode int mode) {
        mCurrentUiMode = mode;

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
    public void onFolderStateSet(@Nullable BookmarkId folder) {
        mCurrentFolder = folder;

        BookmarkItem folderItem =
                mCurrentFolder == null ? null : mBookmarkModel.getBookmarkById(mCurrentFolder);
        mModel.set(
                BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE,
                folderItem != null && folderItem.isEditable());
        if (folderItem == null) return;

        assumeNonNull(mCurrentFolder);

        // Title, navigation buttons.
        String title;
        @NavigationButton int navigationButton;
        Resources res = mContext.getResources();
        if (mCurrentFolder.equals(mBookmarkModel.getRootFolderId())) {
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
        boolean inReadingList = mBookmarkModel.isReadingListFolder(mCurrentFolder);
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
        int numSelected = selectedBookmarks.size();
        boolean hasFolder = false;
        boolean hasPartnerBookmark = false;
        boolean hasOnlyReadingListItems = true;
        boolean hasSelection = numSelected > 0;
        int numRead = 0;

        for (BookmarkId bookmark : selectedBookmarks) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(bookmark);
            if (item == null) continue;

            if (item.isFolder()) hasFolder = true;
            if (bookmark.getType() == BookmarkType.PARTNER) hasPartnerBookmark = true;
            if (bookmark.getType() == BookmarkType.READING_LIST) {
                if (item.isRead()) numRead++;
            } else {
                hasOnlyReadingListItems = false;
            }
        }

        // Partner bookmarks can't move or be edited, so if the selection includes a partner
        // bookmark, disable the move and edit buttons. Only one bookmark can be edited at a time,
        // but any non-zero amount can be moved at a time.
        boolean showEdit = !hasPartnerBookmark && numSelected == 1;
        boolean showMove = !hasPartnerBookmark && numSelected > 0;

        // It does not make sense to open a folder in new tab or copy a folder link, so do not show
        // these options if any folder is part of the selection. Only one link can be copied at a
        // time, but any amount can be opened; to open in incognito it must be enabled.
        boolean showCopyLink = !hasFolder && numSelected == 1;
        boolean showOpenInNewTab = !hasFolder && numSelected > 0;
        boolean showOpenInIncognito =
                !hasFolder && numSelected > 0 && mIncognitoEnabledSupplier.getAsBoolean();

        // Only show the "mark as" options when all selections are reading list items and
        // have the same read state; 'Read' when all items are unread, and vice versa.
        boolean showMarkRead = hasOnlyReadingListItems && hasSelection && numRead == 0;
        boolean showMarkUnread = hasOnlyReadingListItems && hasSelection && numRead == numSelected;

        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT, showEdit);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB, showOpenInNewTab);
        mModel.set(
                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                showOpenInIncognito);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE, showMove);
        mModel.set(BookmarkToolbarProperties.SELECTION_MODE_SHOW_COPY_LINK, showCopyLink);
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
        return Resources.ID_NULL;
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
        return Resources.ID_NULL;
    }

    // Private methods.

    @RequiresNonNull("mBookmarkDelegate")
    private void onNavigateBack() {
        if (mCurrentUiMode == BookmarkUiMode.SEARCHING) {
            mEndSearchRunnable.run();
            return;
        }

        mBookmarkDelegate.openFolder(
                assumeNonNull(mBookmarkModel.getBookmarkById(mCurrentFolder)).getParentId());
    }
}
