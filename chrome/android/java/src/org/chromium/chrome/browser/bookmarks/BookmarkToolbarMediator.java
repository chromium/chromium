// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
class BookmarkToolbarMediator implements BookmarkUiObserver,
                                         DragReorderableListAdapter.DragListener,
                                         SelectionDelegate.SelectionObserver<BookmarkItem> {
    private final Context mContext;
    private final PropertyModel mModel;
    private final BookmarkItemsAdapter mBookmarkItemsAdapter;
    private final SelectionDelegate mSelectionDelegate;
    private final BookmarkModel mBookmarkModel;

    // TODO(crbug.com/1413463): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    private BookmarkId mCurrentFolder;

    BookmarkToolbarMediator(Context context, PropertyModel model,
            BookmarkItemsAdapter bookmarkItemsAdapter,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier,
            SelectionDelegate selectionDelegate, BookmarkModel bookmarkModel) {
        mContext = context;
        mModel = model;
        mBookmarkItemsAdapter = bookmarkItemsAdapter;
        mBookmarkItemsAdapter.addDragListener(this);
        mSelectionDelegate = selectionDelegate;
        mSelectionDelegate.addObserver(this);
        mBookmarkModel = bookmarkModel;

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

    // BookmarkUiObserver implementation.

    @Override
    public void onDestroy() {
        mBookmarkItemsAdapter.removeDragListener(this);
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
        } else if (mBookmarkModel.getTopLevelFolderParentIDs().contains(folderItem.getParentId())
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