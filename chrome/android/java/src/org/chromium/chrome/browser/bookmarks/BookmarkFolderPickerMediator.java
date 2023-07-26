// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Mediator for the folder picker activity. */
class BookmarkFolderPickerMediator {
    static final int FOLDER_ROW = 1;

    private final BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            if (mBookmarkModel.doesBookmarkExist(mBookmarkId)) {
                populateFoldersForParentId(mCurrentParentItem == null ? mBookmarkItem.getParentId()
                                                                      : mCurrentParentItem.getId());
            } else {
                mFinishRunnable.run();
            }
        }
    };

    // Binds properties to the view.
    private final PropertyModel mModel;
    // Binds items to the recycler view.
    private final ModelList mModelList;
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final BookmarkId mBookmarkId;
    private final BookmarkItem mBookmarkItem;
    private final Runnable mFinishRunnable;
    private final BookmarkQueryHandler mQueryHandler;
    private final BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;

    private MenuItem mCreateNewFolderMenu;
    private BookmarkItem mCurrentParentItem;

    BookmarkFolderPickerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkImageFetcher bookmarkImageFetcher, BookmarkId bookmarkId,
            Runnable finishRunnable, BookmarkUiPrefs bookmarkUiPrefs, PropertyModel model,
            ModelList modelList, BookmarkAddNewFolderCoordinator addNewFolderCoordinator) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkId = bookmarkId;
        mFinishRunnable = finishRunnable;
        mQueryHandler = new ImprovedBookmarkQueryHandler(mBookmarkModel, bookmarkUiPrefs);
        mModel = model;
        mModelList = modelList;
        mAddNewFolderCoordinator = addNewFolderCoordinator;

        mModel.set(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER, this::onCancelClicked);
        mModel.set(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER, this::onMoveClicked);

        mBookmarkItem = mBookmarkModel.getBookmarkById(mBookmarkId);
        if (!mBookmarkModel.doesBookmarkExist(mBookmarkId) || mBookmarkItem == null) {
            mFinishRunnable.run();
            return;
        }

        mBookmarkModel.finishLoadingBookmarkModel(
                () -> { populateFoldersForParentId(mBookmarkItem.getParentId()); });
    }

    void destroy() {
        mBookmarkModel.removeObserver(mBookmarkModelObserver);
    }

    void populateFoldersForParentId(BookmarkId parentId) {
        BookmarkItem parentItem = mBookmarkModel.getBookmarkById(parentId);
        mCurrentParentItem = parentItem;
        updateToolbarTitleForCurrentParent();
        updateButtonsForCurrentParent();

        List<BookmarkListEntry> children =
                mQueryHandler.buildBookmarkListForParent(parentItem.getId());

        mModelList.clear();
        for (int i = 0; i < children.size(); i++) {
            BookmarkItem childItem = children.get(i).getBookmarkItem();
            if (childItem == null) continue;

            // Filter out everything that's not a folder.
            if (!childItem.isFolder()) {
                continue;
            }
            // Filter out the bookmark being moved.
            if (childItem.getId().equals(mBookmarkId)) {
                continue;
            }
            mModelList.add(createFolderPickerRow(childItem));
        }
    }

    ListItem createFolderPickerRow(BookmarkItem item) {
        PropertyModel model =
                new PropertyModel.Builder(BookmarkFolderPickerRowProperties.ALL_KEYS)
                        .with(BookmarkFolderPickerRowProperties.ROW_COORDINATOR,
                                new ImprovedBookmarkFolderSelectRowCoordinator(mContext,
                                        mBookmarkImageFetcher, mBookmarkModel,
                                        () -> { populateFoldersForParentId(item.getId()); }))
                        .build();
        model.get(BookmarkFolderPickerRowProperties.ROW_COORDINATOR).setBookmarkId(item.getId());
        return new ListItem(FOLDER_ROW, model);
    }

    void updateToolbarTitleForCurrentParent() {
        String title;
        if (mCurrentParentItem.getId().equals(mBookmarkModel.getRootFolderId())) {
            title = mContext.getString(R.string.folder_picker_root);
        } else {
            title = mCurrentParentItem.getTitle();
        }
        mModel.set(BookmarkFolderPickerProperties.TOOLBAR_TITLE, title);
    }

    void updateButtonsForCurrentParent() {
        boolean moveFolder = mBookmarkItem.isFolder()
                && BookmarkUtils.canAddFolderWhileViewingParent(
                        mBookmarkModel, mCurrentParentItem.getId());
        boolean moveBookmark = !mBookmarkItem.isFolder()
                && BookmarkUtils.canAddBookmarkWhileViewingParent(
                        mBookmarkModel, mCurrentParentItem.getId());
        mModel.set(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED, moveFolder || moveBookmark);
    }

    // Delegate methods for embedder.

    void createOptionsMenu(Menu menu) {
        Drawable icon = UiUtils.getTintedDrawable(mContext, R.drawable.ic_create_new_folder_24dp,
                R.color.default_icon_color_tint_list);
        mCreateNewFolderMenu = menu.add(R.string.create_new_folder)
                                       .setIcon(icon)
                                       .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_ALWAYS);
    }

    boolean optionsItemSelected(MenuItem item) {
        if (item == mCreateNewFolderMenu) {
            mAddNewFolderCoordinator.show(mCurrentParentItem.getId());
            return true;
        } else if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return false;
    }

    boolean onBackPressed() {
        if (mCurrentParentItem.getId().equals(mBookmarkModel.getRootFolderId())) {
            mFinishRunnable.run();
        } else {
            populateFoldersForParentId(mCurrentParentItem.getParentId());
        }

        return true;
    }

    // Private methods.

    private void onCancelClicked(View v) {
        mFinishRunnable.run();
    }

    private void onMoveClicked(View v) {
        BookmarkUtils.moveBookmarksToViewedParent(mBookmarkModel,
                new ArrayList<>(Arrays.asList(mBookmarkId)), mCurrentParentItem.getId());
        mFinishRunnable.run();
    }
}
