// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View.OnClickListener;

import androidx.annotation.IdRes;
import androidx.appcompat.widget.Toolbar.OnMenuItemClickListener;
import androidx.core.view.MenuCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.util.ToolbarUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

/**
 * Main toolbar of bookmark UI. It is responsible for displaying title and buttons associated with
 * the current context.
 */
public class BookmarkToolbar extends SelectableListToolbar<BookmarkId>
        implements OnMenuItemClickListener, OnClickListener {
    private BookmarkOpener mBookmarkOpener;
    private SelectionDelegate<BookmarkId> mSelectionDelegate;

    private boolean mEditButtonVisible;
    private boolean mNewFolderButtonVisible;
    private boolean mNewFolderButtonEnabled;
    private boolean mSelectionShowEdit;
    private boolean mSelectionShowOpenInNewTab;
    private boolean mSelectionShowOpenInIncognito;
    private boolean mSelectionShowMove;
    private boolean mSelectionShowMarkRead;
    private boolean mSelectionShowMarkUnread;

    private List<Integer> mSortMenuIds;
    private boolean mSortMenuIdsEnabled;

    private Runnable mNavigateBackRunnable;
    private Function<Integer, Boolean> mMenuIdClickedFunction;

    public BookmarkToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        setNavigationOnClickListener(this);

        inflateMenu(R.menu.bookmark_toolbar_menu_improved);
        MenuCompat.setGroupDividerEnabled(
                getMenu().findItem(R.id.normal_options_submenu).getSubMenu(), true);

        setOnMenuItemClickListener(this);
    }

    void setBookmarkOpener(BookmarkOpener bookmarkOpener) {
        mBookmarkOpener = bookmarkOpener;
    }

    void setSelectionDelegate(SelectionDelegate selectionDelegate) {
        mSelectionDelegate = selectionDelegate;
        getMenu().setGroupEnabled(R.id.selection_mode_menu_group, true);
    }

    void setBookmarkUiMode(@BookmarkUiMode int mode) {
        if (mode != BookmarkUiMode.LOADING) {
            showNormalView();
        }
    }

    void setSoftKeyboardVisible(boolean visible) {
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

    void setEditButtonVisible(boolean visible) {
        mEditButtonVisible = visible;
        getMenu().findItem(R.id.edit_menu_id).setVisible(visible);
    }

    void setNewFolderButtonVisible(boolean visible) {
        mNewFolderButtonVisible = visible;
        getMenu().findItem(R.id.create_new_folder_menu_id).setVisible(visible);
    }

    void setNewFolderButtonEnabled(boolean enabled) {
        mNewFolderButtonEnabled = enabled;
        getMenu().findItem(R.id.create_new_folder_menu_id).setEnabled(enabled);
    }

    void setSelectionShowEdit(boolean show) {
        mSelectionShowEdit = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_mode_edit_menu_id).setVisible(show);
    }

    void setSelectionShowOpenInNewTab(boolean show) {
        mSelectionShowOpenInNewTab = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_open_in_new_tab_id).setVisible(show);
    }

    void setSelectionShowOpenInIncognito(boolean show) {
        mSelectionShowOpenInIncognito = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_open_in_incognito_tab_id).setVisible(show);
    }

    void setSelectionShowMove(boolean show) {
        mSelectionShowMove = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.selection_mode_move_menu_id).setVisible(show);
    }

    void setSelectionShowMarkRead(boolean show) {
        mSelectionShowMarkRead = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.reading_list_mark_as_read_id).setVisible(show);
    }

    void setSelectionShowMarkUnread(boolean show) {
        mSelectionShowMarkUnread = show;
        if (show) assert mIsSelectionEnabled;
        getMenu().findItem(R.id.reading_list_mark_as_unread_id).setVisible(show);
    }

    void setNavigationButtonState(@NavigationButton int navigationButtonState) {
        setNavigationButton(navigationButtonState);
    }

    void setCheckedSortMenuId(@IdRes int id) {
        getMenu().findItem(id).setChecked(true);
    }

    void setSortMenuIds(List<Integer> sortMenuIds) {
        mSortMenuIds = sortMenuIds;
    }

    void setSortMenuIdsEnabled(boolean enabled) {
        mSortMenuIdsEnabled = enabled;
        for (Integer id : mSortMenuIds) {
            getMenu().findItem(id).setEnabled(enabled);
        }
    }

    void setCheckedViewMenuId(@IdRes int id) {
        getMenu().findItem(id).setChecked(true);
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
        // The navigation button shouldn't be visible unless the current folder is non-null.
        mNavigateBackRunnable.run();
    }

    @Override
    protected void showNormalView() {
        super.showNormalView();

        // SelectableListToolbar will show/hide the entire group.
        setEditButtonVisible(mEditButtonVisible);
        setNewFolderButtonVisible(mNewFolderButtonVisible);
        setNewFolderButtonEnabled(mNewFolderButtonEnabled);
        setSortMenuIdsEnabled(mSortMenuIdsEnabled);
    }

    @Override
    protected void showSelectionView(List<BookmarkId> selectedItems, boolean wasSelectionEnabled) {
        super.showSelectionView(selectedItems, wasSelectionEnabled);

        setSelectionShowEdit(mSelectionShowEdit);
        setSelectionShowOpenInNewTab(mSelectionShowOpenInNewTab);
        setSelectionShowOpenInIncognito(mSelectionShowOpenInIncognito);
        setSelectionShowMove(mSelectionShowMove);
        setSelectionShowMarkRead(mSelectionShowMarkRead);
        setSelectionShowMarkUnread(mSelectionShowMarkUnread);
    }
}
