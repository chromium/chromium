// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
class BookmarkToolbarCoordinator {
    private final BookmarkToolbar mToolbar;
    private final BookmarkToolbarMediator mMediator;
    private final PropertyModel mModel;

    BookmarkToolbarCoordinator(SelectableListLayout<BookmarkId> selectableListLayout,
            SelectionDelegate selectionDelegate, SearchDelegate searchDelegate,
            BookmarkItemsAdapter bookmarkItemsAdapter, boolean isDialogUi) {
        mToolbar = (BookmarkToolbar) selectableListLayout.initializeToolbar(
                R.layout.bookmark_toolbar, selectionDelegate, 0, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, null, isDialogUi);
        mToolbar.initializeSearchView(
                searchDelegate, R.string.bookmark_toolbar_search, R.id.search_menu_id);

        mModel = new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS).build();
        mModel.set(BookmarkToolbarProperties.DRAG_REORDERABLE_LIST_ADAPTER, bookmarkItemsAdapter);
        mModel.set(BookmarkToolbarProperties.BOOKMARK_UI_STATE, BookmarkUIState.STATE_LOADING);
        mMediator = new BookmarkToolbarMediator(mModel);
    }

    void initialize(BookmarkDelegate bookmarkDelegate) {
        mMediator.initialize(bookmarkDelegate);
        mModel.set(BookmarkToolbarProperties.BOOKMARK_DELEGATE, bookmarkDelegate);
        PropertyModelChangeProcessor.create(mModel, mToolbar, BookmarkToolbarViewBinder::bind);
    }

    // Testing methods

    public BookmarkToolbar getToolbarForTesting() {
        return mToolbar;
    }
}