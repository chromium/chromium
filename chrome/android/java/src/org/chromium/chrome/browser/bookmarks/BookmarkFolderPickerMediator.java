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

    private boolean mMovingAtLeastOneFolder;
    private boolean mMovingAtLeastOneBookmark;
    private MenuItem mCreateNewFolderMenu;
    private BookmarkItem mCurrentParentItem;

    BookmarkFolderPickerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkImageFetcher bookmarkImageFetcher, List<BookmarkId> bookmarkIds,
            Runnable finishRunnable, BookmarkUiPrefs bookmarkUiPrefs, PropertyModel model,
            ModelList modelList, BookmarkAddNewFolderCoordinator addNewFolderCoordinator) {
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

        mInitialParentId = mBookmarkIds.size() == 1
                ? mBookmarkModel.getBookmarkById(mBookmarkIds.get(0)).getParentId()
                : mBookmarkModel.getRootFolderId();

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
                    && !BookmarkUtils.canAddFolderWhileViewingParent(
                            mBookmarkModel, childItem.getId());
            boolean excludeForBookmark = mMovingAtLeastOneBookmark
                    && !BookmarkUtils.canAddBookmarkWhileViewingParent(
                            mBookmarkModel, childItem.getId());
            // Remove any folders which can't have children added to them.
            if (excludeForFolder || excludeForBookmark) {
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
        mModel.set(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED,
                mBookmarkIds.size() > 1 || !mCurrentParentItem.getId().equals(mInitialParentId));
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
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, new ArrayList<>(mBookmarkIds), mCurrentParentItem.getId());
        mFinishRunnable.run();
    }
}
