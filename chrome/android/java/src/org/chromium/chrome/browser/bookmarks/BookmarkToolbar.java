// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.core.view.MenuCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
/**
 * Main toolbar of bookmark UI. It is responsible for displaying title and buttons
 * associated with the current context.
 */
public class BookmarkToolbar extends SelectableListToolbar<BookmarkId>
        implements OnMenuItemClickListener, OnClickListener {
    // TODO(crbug.com/1425201): Remove BookmarkModel reference.
    private BookmarkModel mBookmarkModel;
    private BookmarkOpener mBookmarkOpener;
    private SelectionDelegate<BookmarkId> mSelectionDelegate;

    // The current folder can be null before being set by the mediator.
    private @Nullable BookmarkItem mCurrentFolder;
    private @BookmarkUiMode int mBookmarkUiMode;
    private boolean mSoftKeyboardVisible;
    // Whether the selection ui is currently showing. This isn't captured by an explicit
    // BookmarkUiMode.
    private boolean mIsSelectionUiShowing;
    private boolean mSearchButtonVisible;
    private boolean mEditButtonVisible;
    private boolean mNewFolderButtonVisible;
    private boolean mNewFolderButtonEnabled;

    private List<Integer> mSortMenuIds;
    private boolean mSortMenuIdsEnabled;

    private Runnable mNavigateBackRunnable;
    private Function<Integer, Boolean> mMenuIdClickedFunction;

    public BookmarkToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setNavigationOnClickListener(this);

        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            inflateMenu(R.menu.bookmark_toolbar_menu_improved);
            MenuCompat.setGroupDividerEnabled(
                    getMenu().findItem(R.id.normal_options_submenu).getSubMenu(), true);
        } else {
            inflateMenu(R.menu.bookmark_toolbar_menu);
        }

        setOnMenuItemClickListener(this);
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
        if (mBookmarkUiMode != BookmarkUiMode.LOADING) {
            showNormalView();
        }

        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            if (mBookmarkUiMode == BookmarkUiMode.SEARCHING) {
                showSearchView(mSoftKeyboardVisible);
            } else {
                hideSearchView(/*notify=*/false);
            }
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

    void setSearchButtonVisible(boolean visible) {
        // The improved bookmarks experience embeds search in the list.
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) return;
        mSearchButtonVisible = visible;
        getMenu().findItem(R.id.search_menu_id).setVisible(visible);
    }

    void setEditButtonVisible(boolean visible) {
        mEditButtonVisible = visible;
        getMenu().findItem(R.id.edit_menu_id).setVisible(visible);
    }

    void setNewFolderButtonVisible(boolean visible) {
        // The new folder button is only visible when improved bookmarks is enabled.
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) return;
        mNewFolderButtonVisible = visible;
        getMenu().findItem(R.id.create_new_folder_menu_id).setVisible(visible);
    }

    void setNewFolderButtonEnabled(boolean enabled) {
        // The new folder button is only visible when improved bookmarks is enabled.
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) return;
        mNewFolderButtonEnabled = enabled;
        getMenu().findItem(R.id.create_new_folder_menu_id).setEnabled(enabled);
    }

    void setNavigationButtonState(@NavigationButton int navigationButtonState) {
        setNavigationButton(navigationButtonState);
    }

    void setCheckedSortMenuId(@IdRes int id) {
        getMenu().findItem(id).setChecked(true);
    }

    void setSortMenuIds(List<Integer> sortMenuIds) {
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) return;
        mSortMenuIds = sortMenuIds;
    }

    void setSortMenuIdsEnabled(boolean enabled) {
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) return;
        mSortMenuIdsEnabled = enabled;
        for (Integer id : mSortMenuIds) {
            getMenu().findItem(id).setEnabled(enabled);
        }
    }

    void setCheckedViewMenuId(@IdRes int id) {
        getMenu().findItem(id).setChecked(true);
    }

    void setCurrentFolder(BookmarkId folder) {
        mCurrentFolder = mBookmarkModel.getBookmarkById(folder);
    }

    void setNavigateBackRunnable(Runnable navigateBackRunnable) {
        mNavigateBackRunnable = navigateBackRunnable;
    }

    void setMenuIdClickedFunction(Function<Integer, Boolean> menuIdClickedFunction) {
        mMenuIdClickedFunction = menuIdClickedFunction;
    }

    void fakeSelectionStateChange() {
        onSelectionStateChange(new ArrayList<>(mSelectionDelegate.getSelectedItems()));
    }

    // OnMenuItemClickListener implementation.

    @Override
    public boolean onMenuItemClick(MenuItem menuItem) {
        hideOverflowMenu();
        return mMenuIdClickedFunction.apply(menuItem.getItemId());
    }

    // SelectableListToolbar implementation.

    @Override
    public void onNavigationBack() {
        if (isSearching()) {
            super.onNavigationBack();
            return;
        }

        // The navigation button shouldn't be visible unless the current folder is non-null.
        mNavigateBackRunnable.run();
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();

        // SelectableListToolbar will show/hide the entire group.
        setSearchButtonVisible(mSearchButtonVisible);
        setEditButtonVisible(mEditButtonVisible);
        setNewFolderButtonVisible(mNewFolderButtonVisible);
        setNewFolderButtonEnabled(mNewFolderButtonEnabled);
        setSortMenuIdsEnabled(mSortMenuIdsEnabled);
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
