// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;

/**
 * Main action bar of bookmark UI. It is responsible for displaying title and buttons
 * associated with the current context.
 */
public class BookmarkActionBar extends SelectableListToolbar<BookmarkId>
        implements BookmarkUIObserver, OnMenuItemClickListener, OnClickListener,
                   DragReorderableListAdapter.DragListener {

    private BookmarkItem mCurrentFolder;
    private BookmarkDelegate mDelegate;

    public BookmarkActionBar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setNavigationOnClickListener(this);
        inflateMenu(R.menu.bookmark_action_bar_menu);
        setOnMenuItemClickListener(this);

        getMenu().findItem(R.id.selection_mode_edit_menu_id).setTitle(R.string.edit_bookmark);
        getMenu().findItem(R.id.selection_mode_move_menu_id)
                .setTitle(R.string.bookmark_action_bar_move);
        getMenu().findItem(R.id.selection_mode_delete_menu_id)
                .setTitle(R.string.bookmark_action_bar_delete);

        getMenu()
                .findItem(R.id.selection_open_in_incognito_tab_id)
                .setTitle(R.string.contextmenu_open_in_incognito_tab);

        // Wait to enable the selection mode group until the BookmarkDelegate is set. The
        // SelectionDelegate is retrieved from the BookmarkDelegate.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, false);
    }

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
            return;
        }

        mDelegate.openFolder(mCurrentFolder.getParentId());
    }

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        hideOverflowMenu();

        if (menuItem.getItemId() == R.id.edit_menu_id) {
            BookmarkAddEditFolderActivity.startEditFolderActivity(getContext(),
                    mCurrentFolder.getId());
            return true;
        } else if (menuItem.getItemId() == R.id.close_menu_id) {
            BookmarkUtils.finishActivityOnPhone(getContext());
            return true;
        } else if (menuItem.getItemId() == R.id.search_menu_id) {
            mDelegate.openSearchUI();
            return true;
        }

        SelectionDelegate<BookmarkId> selectionDelegate = mDelegate.getSelectionDelegate();
        if (menuItem.getItemId() == R.id.selection_mode_edit_menu_id) {
            List<BookmarkId> list = selectionDelegate.getSelectedItemsAsList();
            assert list.size() == 1;
            BookmarkItem item = mDelegate.getModel().getBookmarkById(list.get(0));
            if (item.isFolder()) {
                BookmarkAddEditFolderActivity.startEditFolderActivity(getContext(), item.getId());
            } else {
                BookmarkUtils.startEditActivity(getContext(), item.getId());
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_move_menu_id) {
            List<BookmarkId> list = selectionDelegate.getSelectedItemsAsList();
            if (list.size() >= 1) {
                BookmarkFolderSelectActivity.startFolderSelectActivity(getContext(),
                        list.toArray(new BookmarkId[list.size()]));
                RecordUserAction.record("MobileBookmarkManagerMoveToFolderBulk");
            }
            return true;
        } else if (menuItem.getItemId() == R.id.selection_mode_delete_menu_id) {
            mDelegate.getModel().deleteBookmarks(
                    selectionDelegate.getSelectedItems().toArray(new BookmarkId[0]));
            RecordUserAction.record("MobileBookmarkManagerDeleteBulk");
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_new_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInNewTab");
            RecordHistogram.recordCount1000Histogram(
                    "Bookmarks.Count.OpenInNewTab", mSelectionDelegate.getSelectedItems().size());
            mDelegate.openBookmarksInNewTabs(
                    selectionDelegate.getSelectedItemsAsList(), /*incognito=*/false);
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_incognito_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInIncognito");
            RecordHistogram.recordCount1000Histogram("Bookmarks.Count.OpenInIncognito",
                    mSelectionDelegate.getSelectedItems().size());
            mDelegate.openBookmarksInNewTabs(
                    selectionDelegate.getSelectedItemsAsList(), /*incognito=*/true);
            return true;
        } else if (menuItem.getItemId() == R.id.reading_list_mark_as_read_id
                || menuItem.getItemId() == R.id.reading_list_mark_as_unread_id) {
            // Handle the seclection "mark as" buttons in the same block because the behavior is
            // the same other than one boolean flip.
            for (int i = 0; i < selectionDelegate.getSelectedItemsAsList().size(); i++) {
                BookmarkId bookmark = selectionDelegate.getSelectedItemsAsList().get(i);
                if (bookmark.getType() != BookmarkType.READING_LIST) continue;

                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmark);
                mDelegate.getModel().setReadStatusForReadingList(bookmarkItem.getUrl(),
                        /*read=*/menuItem.getItemId() == R.id.reading_list_mark_as_read_id);
            }
            selectionDelegate.clearSelection();
            return true;
        }

        assert false : "Unhandled menu click.";
        return false;
    }

    void showLoadingUi() {
        setTitle(null);
        setNavigationButton(NAVIGATION_BUTTON_NONE);
        getMenu().findItem(R.id.search_menu_id).setVisible(false);
        getMenu().findItem(R.id.edit_menu_id).setVisible(false);
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();

        if (mDelegate == null) {
            getMenu().findItem(R.id.search_menu_id).setVisible(false);
            getMenu().findItem(R.id.edit_menu_id).setVisible(false);
        }
    }

    /**
     * Sets the delegate to use to handle UI actions related to this action bar.
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     */
    public void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        if (!delegate.isDialogUi()) getMenu().removeItem(R.id.close_menu_id);

        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, true);
    }

    // BookmarkUIObserver implementations.

    @Override
    public void onDestroy() {
        if (mDelegate == null) return;

        mDelegate.removeUIObserver(this);
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        mCurrentFolder = mDelegate.getModel().getBookmarkById(folder);
        getMenu().findItem(R.id.search_menu_id).setVisible(true);
        getMenu().findItem(R.id.edit_menu_id).setVisible(mCurrentFolder.isEditable());

        // If this is the root folder, we can't go up anymore.
        if (folder.equals(mDelegate.getModel().getRootFolderId())) {
            setTitle(R.string.bookmarks);
            setNavigationButton(NAVIGATION_BUTTON_NONE);
            return;
        }

        if (folder.equals(BookmarkId.SHOPPING_FOLDER)) {
            setTitle(R.string.price_tracking_bookmarks_filter_title);
        } else if (mDelegate.getModel().getTopLevelFolderParentIDs().contains(
                           mCurrentFolder.getParentId())
                && TextUtils.isEmpty(mCurrentFolder.getTitle())) {
            setTitle(R.string.bookmarks);
        } else {
            setTitle(mCurrentFolder.getTitle());
        }

        setNavigationButton(NAVIGATION_BUTTON_BACK);
    }

    @Override
    public void onSearchStateSet() {}

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        super.onSelectionStateChange(selectedBookmarks);

        // The super class registers itself as a SelectionObserver before
        // #onBookmarkDelegateInitialized() is called. Return early if mDelegate has not been set.
        if (mDelegate == null) return;

        if (mIsSelectionEnabled) {
            // Editing a bookmark action on multiple selected items doesn't make sense. So disable.
            getMenu().findItem(R.id.selection_mode_edit_menu_id).setVisible(
                    selectedBookmarks.size() == 1);
            getMenu()
                    .findItem(R.id.selection_open_in_incognito_tab_id)
                    .setVisible(IncognitoUtils.isIncognitoModeEnabled());

            // It does not make sense to open a folder in new tab.
            for (BookmarkId bookmark : selectedBookmarks) {
                BookmarkItem item = mDelegate.getModel().getBookmarkById(bookmark);
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

            // Unless type-swapping is enabled disable edit/move buttons. For multi-selections,
            // check that the read state matches for all items before showing "mark as" buttons.
            // Disable move/edit buttons regardless if there's also a partner bookmark selected.
            boolean typeSwappingEnabled = ReadingListFeatures.shouldAllowBookmarkTypeSwapping();
            // Compute whether all selected bookmarks are reading list items and add up the number
            // of read items.
            int numReadingListItems = 0;
            int numRead = 0;
            for (int i = 0; i < selectedBookmarks.size(); i++) {
                BookmarkId bookmark = selectedBookmarks.get(i);
                BookmarkItem bookmarkItem = mDelegate.getModel().getBookmarkById(bookmark);
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
                        .setVisible(typeSwappingEnabled && !hasPartnerBoomarkSelected);
                getMenu()
                        .findItem(R.id.selection_mode_edit_menu_id)
                        .setVisible(selectedBookmarks.size() == 1 && typeSwappingEnabled
                                && !hasPartnerBoomarkSelected);

                // Check the reading list flag before "open in" items.
                boolean shouldUseRegularTab = !ReadingListFeatures.shouldUseCustomTab();
                getMenu()
                        .findItem(R.id.selection_open_in_new_tab_id)
                        .setVisible(shouldUseRegularTab);
                getMenu()
                        .findItem(R.id.selection_open_in_incognito_tab_id)
                        .setVisible(shouldUseRegularTab);
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
        } else {
            mDelegate.notifyStateChange(this);
        }
    }

    // DragListener implementation.

    /**
     * Called when there is a drag in the bookmarks list.
     *
     * @param drag Whether drag is currently on.
     */
    @Override
    public void onDragStateChange(boolean drag) {
        // Disable menu items while dragging.
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, !drag);
        ToolbarUtils.setOverFlowMenuEnabled(this, !drag);

        // Disable listeners while dragging.
        setNavigationOnClickListener(drag ? null : this);
        setOnMenuItemClickListener(drag ? null : this);
    }
}
