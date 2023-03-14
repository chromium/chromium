// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
// TODO(crbug.com/1413463): Write unittests for this class.
class BookmarkToolbarMediator
        implements BookmarkUiObserver, DragReorderableListAdapter.DragListener {
    private final PropertyModel mModel;
    private final BookmarkItemsAdapter mBookmarkItemsAdapter;

    // TODO(crbug.com/1413463): Remove reference to BookmarkDelegate if possible.
    private @Nullable BookmarkDelegate mBookmarkDelegate;

    BookmarkToolbarMediator(PropertyModel model, BookmarkItemsAdapter bookmarkItemsAdapter,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier) {
        mModel = model;
        mBookmarkItemsAdapter = bookmarkItemsAdapter;
        mBookmarkItemsAdapter.addDragListener(this);

        bookmarkDelegateSupplier.onAvailable((bookmarkDelegate) -> {
            mBookmarkDelegate = bookmarkDelegate;
            mModel.set(BookmarkToolbarProperties.OPEN_SEARCH_UI_RUNNABLE,
                    mBookmarkDelegate::openSearchUi);
            mModel.set(
                    BookmarkToolbarProperties.OPEN_FOLDER_CALLBACK, mBookmarkDelegate::openFolder);
            mBookmarkDelegate.addUiObserver(this);
        });
    }

    // BookmarkUiObserver implementation.

    @Override
    public void onDestroy() {
        mBookmarkItemsAdapter.removeDragListener(this);

        if (mBookmarkDelegate != null) {
            mBookmarkDelegate.removeUiObserver(this);
        }
    }

    @Override
    public void onUiModeChanged(int mode) {
        mModel.set(BookmarkToolbarProperties.BOOKMARK_UI_STATE, mode);
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        mModel.set(BookmarkToolbarProperties.CURRENT_FOLDER, folder);
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
}