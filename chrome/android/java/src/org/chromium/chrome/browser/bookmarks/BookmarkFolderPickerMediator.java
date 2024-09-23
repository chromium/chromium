// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/** Mediator for the folder picker activity. */
class BookmarkFolderPickerMediator {
    private final BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {
                    if (mBookmarkModel.doAllBookmarksExist(mBookmarkIds)) {
                        populateFoldersForParentId(
                                mCurrentParentItem == null
                                        ? mOriginalParentId
                                        : mCurrentParentItem.getId());
                    } else {
                        mFinishRunnable.run();
                    }
                }
            };

    private final BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver =
            new BookmarkUiPrefs.Observer() {
                @Override
                public void onBookmarkRowDisplayPrefChanged(
                        @BookmarkRowDisplayPref int displayPref) {
                    populateFoldersForParentId(mCurrentParentItem.getId());
                }
            };

    // Binds properties to the view.
    private final PropertyModel mModel;
    // Binds items to the recycler view.
    private final ModelList mModelList;
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final List<BookmarkId> mBookmarkIds;
    // The original parent id shared by the bookmark ids being moved. Null if the bookmark ids
    // don't share an immediate parent.
    private final @Nullable BookmarkId mOriginalParentId;
    private final boolean mAllMovedBookmarksMatchParent;
    private final Runnable mFinishRunnable;
    private final BookmarkQueryHandler mQueryHandler;
    private final BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    private final ImprovedBookmarkRowCoordinator mImprovedBookmarkRowCoordinator;
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    private boolean mMovingAtLeastOneFolder;
    private boolean mMovingAtLeastOneBookmark;
    private boolean mCanMoveAllToReadingList;
    private BookmarkItem mCurrentParentItem;

    BookmarkFolderPickerMediator(
            Context context,
            BookmarkModel bookmarkModel,
            List<BookmarkId> bookmarkIds,
            Runnable finishRunnable,
            BookmarkUiPrefs bookmarkUiPrefs,
            PropertyModel model,
            ModelList modelList,
            BookmarkAddNewFolderCoordinator addNewFolderCoordinator,
            ImprovedBookmarkRowCoordinator improvedBookmarkRowCoordinator,
            ShoppingService shoppingService) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkIds = bookmarkIds;
        mBookmarkIds.removeIf(id -> mBookmarkModel.getBookmarkById(id) == null);
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
        BookmarkId firstParentId =
                mBookmarkModel.getBookmarkById(mBookmarkIds.get(0)).getParentId();
        List<BookmarkItem> bookmarkItems = new ArrayList<>();
        for (BookmarkId id : mBookmarkIds) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(id);
            bookmarkItems.add(item);
            if (item.isFolder()) {
                mMovingAtLeastOneFolder = true;
            } else {
                mMovingAtLeastOneBookmark = true;
            }

            // If all of the bookmarks being moved have the same parent, then that's used for the
            // initial parent. Don't break early here to correctly populate variables above.
            if (!Objects.equals(firstParentId, item.getParentId())) {
                allMovedBookmarksMatchParent = false;
            }
        }
        mAllMovedBookmarksMatchParent = allMovedBookmarksMatchParent;
        mOriginalParentId =
                mAllMovedBookmarksMatchParent ? firstParentId : mBookmarkModel.getRootFolderId();

        // If all bookmarks have the same parent, then a that bookmark is selected to populate
        // children from. This means that the initial bookmarks shown in the folder picker will be
        // siblings to the original parent.
        BookmarkId bookmarkIdToShow =
                mAllMovedBookmarksMatchParent
                        ? mBookmarkModel.getBookmarkById(firstParentId).getParentId()
                        : mBookmarkModel.getRootFolderId();

        mModel.set(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER, mFinishRunnable);
        mModel.set(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER, this::onMoveClicked);

        // TODO(crbug.com/324303006): Assert that the bookmark model is loaded instead.
        mBookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    mCanMoveAllToReadingList =
                            bookmarkItems.stream()
                                    .map(item -> item.getUrl())
                                    .allMatch(ReadingListUtils::isReadingListSupported);
                    populateFoldersForParentId(bookmarkIdToShow);
                });
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
                mQueryHandler.buildBookmarkListForFolderSelect(parentItem.getId());
        children =
                children.stream().filter(this::filterMovingBookmarks).collect(Collectors.toList());

        mModelList.clear();
        for (int i = 0; i < children.size(); i++) {
            BookmarkListEntry child = children.get(i);
            @ViewType int viewType = child.getViewType();
            if (viewType == ViewType.SECTION_HEADER) {
                mModelList.add(createSectionHeaderRow(child));
            } else {
                mModelList.add(createFolderPickerRow(child));
            }
        }
    }

    private boolean filterMovingBookmarks(BookmarkListEntry entry) {
        BookmarkItem item = entry.getBookmarkItem();
        // Allow non-bookmarks.
        if (item == null) {
            return true;
        }

        return !mBookmarkIds.contains(item.getId());
    }

    ListItem createSectionHeaderRow(BookmarkListEntry entry) {
        PropertyModel propertyModel = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, entry);
        return new ListItem(entry.getViewType(), propertyModel);
    }

    ListItem createFolderPickerRow(BookmarkListEntry entry) {
        BookmarkItem bookmarkItem = entry.getBookmarkItem();
        BookmarkId bookmarkId = bookmarkItem.getId();

        PropertyModel propertyModel =
                mImprovedBookmarkRowCoordinator.createBasePropertyModel(bookmarkId);

        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, entry);
        propertyModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_RES, R.drawable.outline_chevron_right_24dp);
        propertyModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        propertyModel.set(
                ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER,
                () -> populateFoldersForParentId(bookmarkId));
        // Intentionally ignore long clicks to prevent selection.
        propertyModel.set(ImprovedBookmarkRowProperties.ROW_LONG_CLICK_LISTENER, () -> true);

        // If the location isn't valid for our specific set of bookmarks, then disable the row.
        propertyModel.set(
                ImprovedBookmarkRowProperties.ENABLED, isValidFolderForMovedBookmarks(bookmarkId));

        return new ListItem(entry.getViewType(), propertyModel);
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
        boolean isInvalidFolderLocation =
                (mMovingAtLeastOneFolder
                        && !BookmarkUtils.canAddFolderToParent(mBookmarkModel, currentParentId));
        boolean isInvalidBookmarkLocation =
                (mMovingAtLeastOneBookmark
                        && !BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, currentParentId));
        boolean isInitialParent =
                mAllMovedBookmarksMatchParent && Objects.equals(currentParentId, mOriginalParentId);
        mModel.set(
                BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED,
                !isInvalidFolderLocation && !isInvalidBookmarkLocation && !isInitialParent);
        updateToolbarButtons();
    }

    void updateToolbarButtons() {
        mModel.set(
                BookmarkFolderPickerProperties.ADD_NEW_FOLDER_BUTTON_ENABLED,
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

    private void onMoveClicked() {
        mBookmarkModel.moveBookmarks(mBookmarkIds, mCurrentParentItem.getId());
        BookmarkUtils.setLastUsedParent(mCurrentParentItem.getId());
        mFinishRunnable.run();
    }

    private boolean isValidFolderForMovedBookmarks(BookmarkId folderId) {
        if (mMovingAtLeastOneFolder) {
            return BookmarkUtils.canAddFolderToParent(mBookmarkModel, folderId);
        } else if (folderId.equals(mBookmarkModel.getAccountReadingListFolder())
                || folderId.equals(mBookmarkModel.getLocalOrSyncableReadingListFolder())) {
            return mCanMoveAllToReadingList;
        } else {
            return BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, folderId);
        }
    }
}
