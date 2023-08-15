// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Mediator for the folder picker activity. */
class BookmarkFolderPickerMediator {
    static final int FOLDER_ROW = 1;

    private final BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            if (mBookmarkModel.doAllBookmarksExist(mBookmarkIds)) {
                populateFoldersForParentId(
                        mCurrentParentItem == null ? mInitialParentId : mCurrentParentItem.getId());
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
    private final List<BookmarkId> mBookmarkIds;
    private final BookmarkId mInitialParentId;
    private final Runnable mFinishRunnable;
    private final BookmarkQueryHandler mQueryHandler;
    private final BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    private final ImprovedBookmarkRowCoordinator mImprovedBookmarkRowCoordinator;

    private boolean mMovingAtLeastOneFolder;
    private boolean mMovingAtLeastOneBookmark;
    private MenuItem mCreateNewFolderMenu;
    private BookmarkItem mCurrentParentItem;

    BookmarkFolderPickerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkImageFetcher bookmarkImageFetcher, List<BookmarkId> bookmarkIds,
            BookmarkId initialParentId, Runnable finishRunnable, BookmarkUiPrefs bookmarkUiPrefs,
            PropertyModel model, ModelList modelList,
            BookmarkAddNewFolderCoordinator addNewFolderCoordinator,
            ImprovedBookmarkRowCoordinator improvedBookmarkRowCoordinator) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkIds = bookmarkIds;
        mFinishRunnable = finishRunnable;
        mQueryHandler = new ImprovedBookmarkQueryHandler(mBookmarkModel, bookmarkUiPrefs);
        mModel = model;
        mModelList = modelList;
        mAddNewFolderCoordinator = addNewFolderCoordinator;
        mImprovedBookmarkRowCoordinator = improvedBookmarkRowCoordinator;

        mInitialParentId = initialParentId;

        for (BookmarkId id : mBookmarkIds) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            if (item.isFolder()) {
                mMovingAtLeastOneFolder = true;
            } else {
                mMovingAtLeastOneBookmark = true;
            }
        }

        mModel.set(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER, this::onCancelClicked);
        mModel.set(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER, this::onMoveClicked);

        mBookmarkModel.finishLoadingBookmarkModel(
                () -> { populateFoldersForParentId(mInitialParentId); });
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
            // Filter out the bookmarks being moved.
            if (mBookmarkIds.contains(childItem.getId())) {
                continue;
            }

            boolean excludeForFolder = mMovingAtLeastOneFolder
                    && !BookmarkUtils.canAddFolderToParent(mBookmarkModel, childItem.getId());
            boolean excludeForBookmark = mMovingAtLeastOneBookmark
                    && !BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, childItem.getId());
            // Remove any folders which can't have children added to them.
            if (excludeForFolder || excludeForBookmark) {
                continue;
            }
            mModelList.add(createFolderPickerRow(childItem));
        }
    }

    ListItem createFolderPickerRow(BookmarkItem bookmarkItem) {
        BookmarkId bookmarkId = bookmarkItem.getId();

        PropertyModel propertyModel =
                mImprovedBookmarkRowCoordinator.createBasePropertyModel(bookmarkId);

        propertyModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_RES, R.drawable.outline_chevron_right_24dp);
        propertyModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        propertyModel.set(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER,
                (v) -> { populateFoldersForParentId(bookmarkId); });

        return new ListItem(FOLDER_ROW, propertyModel);
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
        mModel.set(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED,
                mBookmarkIds.size() > 1 || !mCurrentParentItem.getId().equals(mInitialParentId));
        updateToolbarButtons();
    }

    void updateToolbarButtons() {
        mModel.set(BookmarkFolderPickerProperties.ADD_NEW_FOLDER_BUTTON_ENABLED,
                BookmarkUtils.canAddFolderToParent(mBookmarkModel, mCurrentParentItem.getId()));
    }

    // Delegate methods for embedder.

    boolean optionsItemSelected(int menuItemId) {
        if (menuItemId == R.id.create_new_folder_menu_id) {
            mAddNewFolderCoordinator.show(mCurrentParentItem.getId());
            return true;
        } else if (menuItemId == android.R.id.home) {
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
        BookmarkUtils.moveBookmarksToParent(
                mBookmarkModel, mBookmarkIds, mCurrentParentItem.getId());
        mFinishRunnable.run();
    }
}
