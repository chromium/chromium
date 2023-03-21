// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;

/**
 * Main toolbar of bookmark UI. It is responsible for displaying title and buttons
 * associated with the current context.
 */
public class BookmarkToolbar extends SelectableListToolbar<BookmarkId>
        implements OnMenuItemClickListener, OnClickListener {
    // TODO(crbug.com/1425201): Remove BookmarkModel reference.
    private BookmarkModel mBookmarkModel;
    private BookmarkOpener mBookmarkOpener;
    private SelectionDelegate mSelectionDelegate;

    // The current folder can be null before being set by the mediator.
    private @Nullable BookmarkItem mCurrentFolder;
    private @BookmarkUiMode int mBookmarkUiMode;
    private boolean mSoftKeyboardVisible;
    // Whether the selection ui is currently showing. This isn't captured by an explicit
    // BookmarkUiMode.
    private boolean mIsSelectionUiShowing;

    private Runnable mOpenSearchUiRunnable;
    private Callback<BookmarkId> mOpenFolderCallback;

    public BookmarkToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setNavigationOnClickListener(this);
        inflateMenu(R.menu.bookmark_toolbar_menu);
        setOnMenuItemClickListener(this);

        getMenu().findItem(R.id.selection_mode_edit_menu_id).setTitle(R.string.edit_bookmark);
        getMenu()
                .findItem(R.id.selection_mode_move_menu_id)
                .setTitle(R.string.bookmark_toolbar_move);
        getMenu()
                .findItem(R.id.selection_mode_delete_menu_id)
                .setTitle(R.string.bookmark_toolbar_delete);

        getMenu()
                .findItem(R.id.selection_open_in_incognito_tab_id)
                .setTitle(R.string.contextmenu_open_in_incognito_tab);

        // Wait to enable the selection mode group until the SelectionDelegate is set.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, false);
    }

    void setBookmarkModel(BookmarkModel bookmarkModel) {
        mBookmarkModel = bookmarkModel;
    }

    void setBookmarkOpener(BookmarkOpener bookmarkOpener) {
        mBookmarkOpener = bookmarkOpener;
    }

    void setSelectionDelegate(SelectionDelegate selectionDelegate) {
        mSelectionDelegate = selectionDelegate;
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, true);
    }

    void setBookmarkUiMode(@BookmarkUiMode int mode) {
        mBookmarkUiMode = mode;
        mIsSelectionUiShowing = false;
        if (mBookmarkUiMode == BookmarkUiMode.LOADING) {
            showLoadingUi();
        } else {
            showNormalView();
        }

        if (mBookmarkUiMode == BookmarkUiMode.SEARCHING) {
            showSearchView(mSoftKeyboardVisible);
        } else {
            hideSearchView(/*notify=*/false);
        }

        if (mBookmarkUiMode == BookmarkUiMode.FOLDER && mCurrentFolder != null) {
            // It's possible that the folder was renamed, so refresh the folder UI just in case.
            setCurrentFolder(mCurrentFolder);
        }
    }

    void setSoftKeyboardVisible(boolean visible) {
        mSoftKeyboardVisible = visible;
        if (!visible) hideKeyboard();
    }

    void setIsDialogUi(boolean isDialogUi) {
        if (!isDialogUi) getMenu().removeItem(R.id.close_menu_id);
    }

    void setDragEnabled(boolean dragEnabled) {
        // Disable menu items while dragging.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, !dragEnabled);
        ToolbarUtils.setOverFlowMenuEnabled(this, !dragEnabled);

        // Disable listeners while dragging.
        setNavigationOnClickListener(dragEnabled ? null : this);
        setOnMenuItemClickListener(dragEnabled ? null : this);
    }

    /** Sets the current folder as a BookmarkId. */
    // TODO(crbug.com/1413463): The individual title/nav state should be set manually instead of
    // being derived from the BookmarkId.
    void setCurrentFolder(BookmarkId folder) {
        setCurrentFolder(mBookmarkModel.getBookmarkById(folder));
    }

    /** Sets the current folder as a BookmarkItem. */
    void setCurrentFolder(BookmarkItem folder) {
        mCurrentFolder = folder;

        getMenu().findItem(R.id.search_menu_id).setVisible(true);
        getMenu().findItem(R.id.edit_menu_id).setVisible(mCurrentFolder.isEditable());

        // If this is the root folder, we can't go up anymore.
        if (folder.getId().equals(mBookmarkModel.getRootFolderId())) {
            setTitle(R.string.bookmarks);
            setNavigationButton(NAVIGATION_BUTTON_NONE);
            return;
        }

        if (folder.getId().equals(BookmarkId.SHOPPING_FOLDER)) {
            setTitle(R.string.price_tracking_bookmarks_filter_title);
        } else if (mBookmarkModel.getTopLevelFolderParentIDs().contains(
                           mCurrentFolder.getParentId())
                && TextUtils.isEmpty(mCurrentFolder.getTitle())) {
            setTitle(R.string.bookmarks);
        } else {
            setTitle(mCurrentFolder.getTitle());
        }

        setNavigationButton(NAVIGATION_BUTTON_BACK);
    }

    void setOpenSearchUiRunnable(Runnable runnable) {
        mOpenSearchUiRunnable = runnable;
    }

    void setOpenFolderCallback(Callback<BookmarkId> openFolderCallback) {
        mOpenFolderCallback = openFolderCallback;
    }

    void showLoadingUi() {
        setTitle(null);
        setNavigationButton(NAVIGATION_BUTTON_NONE);
        getMenu().findItem(R.id.search_menu_id).setVisible(false);
        getMenu().findItem(R.id.edit_menu_id).setVisible(false);
    }

    // OnMenuItemClickListener implementation.

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        hideOverflowMenu();

        if (menuItem.getItemId() == R.id.edit_menu_id) {
            BookmarkAddEditFolderActivity.startEditFolderActivity(
                    getContext(), mCurrentFolder.getId());
            return true;
        } else if (menuItem.getItemId() == R.id.close_menu_id) {
            BookmarkUtils.finishActivityOnPhone(getContext());
            return true;
        } else if (menuItem.getItemId() == R.id.search_menu_id) {
            mOpenSearchUiRunnable.run();
            return true;
        }

        if (menuItem.getItemId() == R.id.selection_mode_edit_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            assert list.size() == 1;
            BookmarkItem item = mBookmarkModel.getBookmarkById(list.get(0));
            if (item.isFolder()) {
                BookmarkAddEditFolderActivity.startEditFolderActivity(getContext(), item.getId());
            } else {
                BookmarkUtils.startEditActivity(getContext(), item.getId());
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_move_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                BookmarkFolderSelectActivity.startFolderSelectActivity(
                        getContext(), list.toArray(new BookmarkId[0]));
                RecordUserAction.record("MobileBookmarkManagerMoveToFolderBulk");
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_delete_menu_id) {
            List<BookmarkId> list = mSelectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                mBookmarkModel.deleteBookmarks(list.toArray(new BookmarkId[0]));
                RecordUserAction.record("MobileBookmarkManagerDeleteBulk");
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_new_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInNewTab");
            RecordHistogram.recordCount1000Histogram(
                    "Bookmarks.Count.OpenInNewTab", mSelectionDelegate.getSelectedItems().size());
            mBookmarkOpener.openBookmarksInNewTabs(
                    mSelectionDelegate.getSelectedItemsAsList(), /*incognito=*/false);
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_incognito_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInIncognito");
            RecordHistogram.recordCount1000Histogram("Bookmarks.Count.OpenInIncognito",
                    mSelectionDelegate.getSelectedItems().size());
            mBookmarkOpener.openBookmarksInNewTabs(
                    mSelectionDelegate.getSelectedItemsAsList(), /*incognito=*/true);
            return true;
        } else if (menuItem.getItemId() == R.id.reading_list_mark_as_read_id
                || menuItem.getItemId() == R.id.reading_list_mark_as_unread_id) {
            // Handle the seclection "mark as" buttons in the same block because the behavior is
            // the same other than one boolean flip.
            for (int i = 0; i < mSelectionDelegate.getSelectedItemsAsList().size(); i++) {
                BookmarkId bookmark =
                        (BookmarkId) mSelectionDelegate.getSelectedItemsAsList().get(i);
                if (bookmark.getType() != BookmarkType.READING_LIST) continue;

                BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmark);
                mBookmarkModel.setReadStatusForReadingList(bookmarkItem.getUrl(),
                        /*read=*/menuItem.getItemId() == R.id.reading_list_mark_as_read_id);
            }
            mSelectionDelegate.clearSelection();
            return true;
        }

        assert false : "Unhandled menu click.";
        return false;
    }

    // SelectableListToolbar implementation.

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
            return;
        }

        mOpenFolderCallback.onResult(mCurrentFolder.getParentId());
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();

        if (mCurrentFolder == null) {
            getMenu().findItem(R.id.search_menu_id).setVisible(false);
            getMenu().findItem(R.id.edit_menu_id).setVisible(false);
        } else {
            setCurrentFolder(mCurrentFolder);
        }
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        super.onSelectionStateChange(selectedBookmarks);

        // The super class registers itself as a SelectionObserver before #setBookmarkModel.
        // Return early if mBookmarkModel has not been set.
        if (mBookmarkModel == null) return;

        if (mIsSelectionEnabled) {
            mIsSelectionUiShowing = true;
            // Editing a bookmark action on multiple selected items doesn't make sense. So disable.
            getMenu()
                    .findItem(R.id.selection_mode_edit_menu_id)
                    .setVisible(selectedBookmarks.size() == 1);
            getMenu()
                    .findItem(R.id.selection_open_in_incognito_tab_id)
                    .setVisible(IncognitoUtils.isIncognitoModeEnabled());

            // It does not make sense to open a folder in new tab.
            for (BookmarkId bookmark : selectedBookmarks) {
                BookmarkItem item = mBookmarkModel.getBookmarkById(bookmark);
                if (item != null && item.isFolder()) {
                    getMenu().findItem(R.id.selection_open_in_new_tab_id).setVisible(false);
                    getMenu().findItem(R.id.selection_open_in_incognito_tab_id).setVisible(false);
                    break;
                }
            }

            boolean hasPartnerBoomarkSelected = false;
            // Partner bookmarks can't move, so if the selection includes a partner bookmark,
            // disable the move button.
            for (BookmarkId bookmark : selectedBookmarks) {
                if (bookmark.getType() == BookmarkType.PARTNER) {
                    hasPartnerBoomarkSelected = true;
                    getMenu().findItem(R.id.selection_mode_move_menu_id).setVisible(false);
                    break;
                }
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

            // Don't show the move/edit buttons if there are also partner bookmarks selected since
            // these bookmarks can't be moved or edited. If there are no reading list items
            // selected, then use default behavior.
            if (numReadingListItems > 0) {
                getMenu()
                        .findItem(R.id.selection_mode_move_menu_id)
                        .setVisible(!hasPartnerBoomarkSelected);
                getMenu()
                        .findItem(R.id.selection_mode_edit_menu_id)
                        .setVisible(selectedBookmarks.size() == 1 && !hasPartnerBoomarkSelected);

                getMenu().findItem(R.id.selection_open_in_new_tab_id).setVisible(true);
                getMenu().findItem(R.id.selection_open_in_incognito_tab_id).setVisible(true);
            }

            // Only show the "mark as" options when all selections are reading list items and
            // have the same read state.
            boolean onlyReadingListSelected =
                    selectedBookmarks.size() > 0 && numReadingListItems == selectedBookmarks.size();
            getMenu()
                    .findItem(R.id.reading_list_mark_as_read_id)
                    .setVisible(onlyReadingListSelected && numRead == 0);
            getMenu()
                    .findItem(R.id.reading_list_mark_as_unread_id)
                    .setVisible(onlyReadingListSelected && numRead == selectedBookmarks.size());
        } else if (mIsSelectionUiShowing) {
            // When selection isn't enabled (e.g. we just de-selected the last item) but the
            // selection UI is still showing we want to revert to the previous known mode.
            setBookmarkUiMode(mBookmarkUiMode);
        }
    }
}
