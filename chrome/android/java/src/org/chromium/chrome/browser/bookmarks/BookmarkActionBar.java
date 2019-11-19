// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content_public.browser.LoadUrlParams;

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
            openBookmarksInNewTabs(selectionDelegate.getSelectedItemsAsList(),
                    new TabDelegate(false), mDelegate.getModel());
            selectionDelegate.clearSelection();
            return true;
        } else if (menuItem.getItemId() == R.id.selection_open_in_incognito_tab_id) {
            RecordUserAction.record("MobileBookmarkManagerEntryOpenedInIncognito");
            RecordHistogram.recordCount1000Histogram("Bookmarks.Count.OpenInIncognito",
                    mSelectionDelegate.getSelectedItems().size());
            openBookmarksInNewTabs(selectionDelegate.getSelectedItemsAsList(),
                    new TabDelegate(true), mDelegate.getModel());
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

        if (mDelegate.getModel().getTopLevelFolderParentIDs().contains(mCurrentFolder.getParentId())
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
            // Partner bookmarks can't move, so if the selection includes a partner bookmark,
            // disable the move button.
            for (BookmarkId bookmark : selectedBookmarks) {
                if (bookmark.getType() == BookmarkType.PARTNER) {
                    getMenu().findItem(R.id.selection_mode_move_menu_id).setVisible(false);
                    break;
                }
            }
        } else {
            mDelegate.notifyStateChange(this);
        }
    }

    private static void openBookmarksInNewTabs(
            List<BookmarkId> bookmarks, TabDelegate tabDelegate, BookmarkModel model) {
        for (BookmarkId id : bookmarks) {
            tabDelegate.createNewTab(new LoadUrlParams(model.getBookmarkById(id).getUrl()),
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
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
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, !drag);
        setNavigationOnClickListener(drag ? null : this);
        setOnMenuItemClickListener(drag ? null : this);
    }
}
