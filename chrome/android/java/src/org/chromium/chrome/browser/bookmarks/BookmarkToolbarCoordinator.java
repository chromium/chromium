// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.function.BooleanSupplier;

/** Responsible for the business logic for the BookmarkManagerToolbar. */
public class BookmarkToolbarCoordinator {
    private final BookmarkToolbar mToolbar;
    private final BookmarkToolbarMediator mMediator;
    private final PropertyModel mModel;

    BookmarkToolbarCoordinator(
            Context context,
            SelectableListLayout<BookmarkId> selectableListLayout,
            SelectionDelegate<BookmarkId> selectionDelegate,
            SearchDelegate searchDelegate,
            DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter,
            boolean isDialogUi,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier,
            BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener,
            BookmarkUiPrefs bookmarkUiPrefs,
            ModalDialogManager modalDialogManager,
            Runnable endSearchRunnable,
            BookmarkMoveSnackbarManager bookmarkMoveSnackbarManager,
            BooleanSupplier incognitoEnabledSupplier) {
        mToolbar =
                (BookmarkToolbar)
                        selectableListLayout.initializeToolbar(
                                R.layout.bookmark_toolbar,
                                selectionDelegate,
                                0,
                                R.id.normal_menu_group,
                                R.id.selection_mode_menu_group,
                                null,
                                isDialogUi);
        mToolbar.initializeSearchView(
                searchDelegate, R.string.bookmark_toolbar_search, R.id.search_menu_id);

        mModel = new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS).build();
        mModel.set(BookmarkToolbarProperties.BOOKMARK_OPENER, bookmarkOpener);
        mModel.set(BookmarkToolbarProperties.SELECTION_DELEGATE, selectionDelegate);
        mModel.set(BookmarkToolbarProperties.BOOKMARK_UI_MODE, BookmarkUiMode.LOADING);
        mModel.set(BookmarkToolbarProperties.IS_DIALOG_UI, isDialogUi);
        mModel.set(BookmarkToolbarProperties.DRAG_ENABLED, false);
        mMediator =
                new BookmarkToolbarMediator(
                        context,
                        mModel,
                        dragReorderableRecyclerViewAdapter,
                        bookmarkDelegateSupplier,
                        selectionDelegate,
                        bookmarkModel,
                        bookmarkOpener,
                        bookmarkUiPrefs,
                        new BookmarkAddNewFolderCoordinator(
                                context, modalDialogManager, bookmarkModel),
                        endSearchRunnable,
                        bookmarkMoveSnackbarManager,
                        incognitoEnabledSupplier);

        PropertyModelChangeProcessor.create(mModel, mToolbar, BookmarkToolbarViewBinder::bind);
    }

    // Testing methods

    public BookmarkToolbar getToolbarForTesting() {
        return mToolbar;
    }
}
