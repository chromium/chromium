// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.MenuItem;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.Objects;

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

    private BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver = new BookmarkUiPrefs.Observer() {
        @Override
        public void onBookmarkRowDisplayPrefChanged(@BookmarkRowDisplayPref int displayPref) {
            populateFoldersForParentId(mCurrentParentItem.getId());
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
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final boolean mAllMovedBookmarksMatchParent;

    private boolean mMovingAtLeastOneFolder;
    private boolean mMovingAtLeastOneBookmark;
    private MenuItem mCreateNewFolderMenu;
    private BookmarkItem mCurrentParentItem;

    BookmarkFolderPickerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkImageFetcher bookmarkImageFetcher, List<BookmarkId> bookmarkIds,
            Runnable finishRunnable, BookmarkUiPrefs bookmarkUiPrefs, PropertyModel model,
            ModelList modelList, BookmarkAddNewFolderCoordinator addNewFolderCoordinator,
            ImprovedBookmarkRowCoordinator improvedBookmarkRowCoordinator,
            ShoppingService shoppingService) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mBookmarkIds = bookmarkIds;
        mFinishRunnable = finishRunnable;
        mQueryHandler =
                new ImprovedBookmarkQueryHandler(mBookmarkModel, bookmarkUiPrefs, shoppingService);
        mModel = model;
        mModelList = modelList;
        mAddNewFolderCoordinator = addNewFolderCoordinator;
        mImprovedBookmarkRowCoordinator = improvedBookmarkRowCoordinator;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);

        boolean allMovedBookmarksMatchParent = true;
        BookmarkId firstParent = mBookmarkModel.getBookmarkById(mBookmarkIds.get(0)).getParentId();
        for (BookmarkId id : mBookmarkIds) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            if (item.isFolder()) {
                mMovingAtLeastOneFolder = true;
            } else {
                mMovingAtLeastOneBookmark = true;
            }

            // If all of the bookmarks being moved have the same parent, then that's used for the
            // initial parent.
            if (!Objects.equals(firstParent, item.getParentId())) {
                allMovedBookmarksMatchParent = false;
            }
        }
        mAllMovedBookmarksMatchParent = allMovedBookmarksMatchParent;
        // TODO(crbug.com/1473755): Implement lowest common ancestor here for the initial parent.
        mInitialParentId =
                mAllMovedBookmarksMatchParent ? firstParent : mBookmarkModel.getRootFolderId();

        mModel.set(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER, this::onCancelClicked);
        mModel.set(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER, this::onMoveClicked);

        mBookmarkModel.finishLoadingBookmarkModel(
                () -> { populateFoldersForParentId(mInitialParentId); });
    }

    void destroy() {
        mBookmarkModel.removeObserver(mBookmarkModelObserver);
        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);
    }

    void populateFoldersForParentId(BookmarkId parentId) {
        BookmarkItem parentItem = mBookmarkModel.getBookmarkById(parentId);
        mCurrentParentItem = parentItem;
        updateToolbarTitleForCurrentParent();
        updateButtonsForCurrentParent();

        List<BookmarkListEntry> children =
                mQueryHandler.buildBookmarkListForParent(parentItem.getId(), /*powerFilter*/ null);

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
        // Intentionally ignore long clicks to prevent selection.
        propertyModel.set(
                ImprovedBookmarkRowProperties.ROW_LONG_CLICK_LISTENER, (v) -> { return true; });

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
        BookmarkId currentParentId = mCurrentParentItem.getId();
        // Folders are removed from the list in {@link #populateFoldersForParentId}, but it's still
        // possible to get to invalid folders through hierarchy navigation (e.g. the root folder
        // by navigating up all the way).
        boolean isInvalidFolderLocation = (mMovingAtLeastOneFolder
                && !BookmarkUtils.canAddFolderToParent(mBookmarkModel, currentParentId));
        boolean isInvalidBookmarkLocation = (mMovingAtLeastOneBookmark
                && !BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, currentParentId));
        boolean isInitialParent =
                mAllMovedBookmarksMatchParent && Objects.equals(currentParentId, mInitialParentId);
        mModel.set(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED,
                !isInvalidFolderLocation && !isInvalidBookmarkLocation && !isInitialParent);
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
        BookmarkUtils.setLastUsedParent(mContext, mCurrentParentItem.getId());
        mFinishRunnable.run();
    }
}
