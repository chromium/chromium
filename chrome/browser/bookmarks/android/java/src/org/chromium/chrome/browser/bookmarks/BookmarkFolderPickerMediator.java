// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Mediator for the folder picker activity. */
@NullMarked
class BookmarkFolderPickerMediator {
    private final BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkModelChanged() {
                    if (mBookmarkModel.doAllBookmarksExist(mBookmarkIds)) {
                        populateFoldersForParentId(
                                mCurrentParentItem == null
                                        ? assertNonNull(mOriginalParentId)
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
                    assumeNonNull(mCurrentParentItem);
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
    private @Nullable BookmarkItem mCurrentParentItem;

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
                new ImprovedBookmarkQueryHandler(
                        mBookmarkModel,
                        bookmarkUiPrefs,
                        shoppingService,
                        /* rootFolderForceVisibleMask= */ BookmarkBarUtils
                                        .isDeviceBookmarkBarCompatible(mContext)
                                ? BookmarkNodeMaskBit.ACCOUNT_AND_LOCAL_BOOKMARK_BAR
                                : BookmarkNodeMaskBit.NONE);
        mModel = model;
        mModelList = modelList;
        mAddNewFolderCoordinator = addNewFolderCoordinator;
        mImprovedBookmarkRowCoordinator = improvedBookmarkRowCoordinator;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);

        boolean allMovedBookmarksMatchParent = true;
        BookmarkId firstParentId =
                assumeNonNull(mBookmarkModel.getBookmarkById(mBookmarkIds.get(0))).getParentId();
        List<BookmarkItem> bookmarkItems = new ArrayList<>();
        for (BookmarkId id : mBookmarkIds) {
            BookmarkItem item = assumeNonNull(mBookmarkModel.getBookmarkById(id));
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
                        ? assumeNonNull(mBookmarkModel.getBookmarkById(firstParentId)).getParentId()
                        : assumeNonNull(mBookmarkModel.getRootFolderId());

        mModel.set(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER, mFinishRunnable);
        mModel.set(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER, this::onMoveClicked);

        // crbug.com/439882814 shows bookmark model is not always loaded by this time.
        mBookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    mCanMoveAllToReadingList = allItemsSupportReadingList(bookmarkItems);
                    populateFoldersForParentId(bookmarkIdToShow);
                });
    }

    private static boolean allItemsSupportReadingList(List<BookmarkItem> bookmarkItems) {
        for (BookmarkItem item : bookmarkItems) {
            if (!BookmarkUtils.isReadingListSupported(item.getUrl())) {
                return false;
            }
        }
        return true;
    }

    void destroy() {
        mBookmarkModel.removeObserver(mBookmarkModelObserver);
        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);
    }

    void populateFoldersForParentId(BookmarkId parentId) {
        BookmarkItem parentItem = mBookmarkModel.getBookmarkById(parentId);
        assert parentItem != null;
        mCurrentParentItem = parentItem;
        updateToolbarTitleForCurrentParent();
        updateButtonsForCurrentParent();

        List<BookmarkListEntry> children = mQueryHandler.buildBookmarkListForFolderSelect(parentId);

        mModelList.clear();
        for (BookmarkListEntry child : children) {
            BookmarkItem item = child.getBookmarkItem();
            // Allow non-bookmarks.
            if (item == null || !mBookmarkIds.contains(item.getId())) {
                mModelList.add(
                        child.getViewType() == ViewType.SECTION_HEADER
                                ? createSectionHeaderRow(child)
                                : createFolderPickerRow(child));
            }
        }
    }

    ListItem createSectionHeaderRow(BookmarkListEntry entry) {
        PropertyModel propertyModel = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, entry);
        return new ListItem(entry.getViewType(), propertyModel);
    }

    ListItem createFolderPickerRow(BookmarkListEntry entry) {
        BookmarkItem bookmarkItem = assumeNonNull(entry.getBookmarkItem());
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

    @RequiresNonNull("mCurrentParentItem")
    private void updateToolbarTitleForCurrentParent() {
        String title;
        if (mCurrentParentItem.getId().equals(mBookmarkModel.getRootFolderId())) {
            title = mContext.getString(R.string.folder_picker_root);
        } else {
            title = mCurrentParentItem.getTitle();
        }
        mModel.set(BookmarkFolderPickerProperties.TOOLBAR_TITLE, title);
    }

    @RequiresNonNull("mCurrentParentItem")
    private void updateButtonsForCurrentParent() {
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
        if (mCurrentParentItem == null) {
            return;
        }
        mModel.set(
                BookmarkFolderPickerProperties.ADD_NEW_FOLDER_BUTTON_ENABLED,
                BookmarkUtils.canAddFolderToParent(mBookmarkModel, mCurrentParentItem.getId()));
    }

    // Delegate methods for embedder.

    boolean optionsItemSelected(int menuItemId) {
        if (menuItemId == R.id.create_new_folder_menu_id) {
            assumeNonNull(mCurrentParentItem);
            mAddNewFolderCoordinator.show(mCurrentParentItem.getId());
            return true;
        } else if (menuItemId == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return false;
    }

    boolean onBackPressed() {
        if (mCurrentParentItem == null
                || mCurrentParentItem.getId().equals(mBookmarkModel.getRootFolderId())) {
            mFinishRunnable.run();
        } else {
            populateFoldersForParentId(mCurrentParentItem.getParentId());
        }

        return true;
    }

    // Private methods.

    private void onMoveClicked() {
        assumeNonNull(mCurrentParentItem);
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
