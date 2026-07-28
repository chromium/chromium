// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;

/** Mediator for the bookmark desktop navigation pane. */
@NullMarked
class BookmarkDesktopNavigationMediator extends BookmarkModelObserver
        implements BookmarkUiObserver {
    private boolean mDestroyed;
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final ModelList mModelList;
    private final BookmarkDelegate mBookmarkDelegate;

    private @Nullable BookmarkId mCurrentFolderId;

    /**
     * Constructor.
     *
     * @param context The current Activity context.
     * @param bookmarkModel The bookmark model.
     * @param modelList The model list to populate.
     * @param bookmarkDelegate The bookmark delegate.
     */
    BookmarkDesktopNavigationMediator(
            Context context,
            BookmarkModel bookmarkModel,
            ModelList modelList,
            BookmarkDelegate bookmarkDelegate) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mModelList = modelList;
        mBookmarkDelegate = bookmarkDelegate;

        mBookmarkDelegate.addUiObserver(this);
        mBookmarkModel.addObserver(this);

        refreshNavigationList();
    }

    /** Destroys the mediator and removes observers. */
    void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        mBookmarkDelegate.removeUiObserver(this);
        mBookmarkModel.removeObserver(this);
    }

    private void refreshNavigationList() {
        mModelList.clear();

        if (!mBookmarkModel.isBookmarkModelLoaded()) {
            return;
        }

        List<BookmarkId> topLevelIds =
                mBookmarkModel.getTopLevelFolderIds(
                        BookmarkNodeMaskBit.ACCOUNT_AND_LOCAL_BOOKMARK_BAR);

        List<BookmarkId> accountFolders = new ArrayList<>();
        List<BookmarkId> localFolders = new ArrayList<>();

        for (BookmarkId id : topLevelIds) {
            if (isMobileFolder(id) && mBookmarkModel.getChildCount(id) == 0) {
                continue;
            }
            if (isAccountFolder(id)) {
                accountFolders.add(id);
            } else {
                localFolders.add(id);
            }
        }

        sortFolders(accountFolders);
        sortFolders(localFolders);

        boolean showHeaders = !accountFolders.isEmpty() && !localFolders.isEmpty();

        if (showHeaders && !accountFolders.isEmpty()) {
            mModelList.add(
                    createHeaderItem(
                            mContext.getString(R.string.account_bookmarks_section_header)));
        }
        for (BookmarkId id : accountFolders) {
            mModelList.add(createFolderItem(id));
        }

        if (showHeaders && !localFolders.isEmpty()) {
            mModelList.add(
                    createHeaderItem(mContext.getString(R.string.local_bookmarks_section_header)));
        }
        for (BookmarkId id : localFolders) {
            mModelList.add(createFolderItem(id));
        }

        updateSelectionHighlight();
    }

    private boolean isAccountFolder(BookmarkId id) {
        BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        return item != null && item.isAccountBookmark();
    }

    private boolean isMobileFolder(BookmarkId id) {
        return Objects.equals(id, mBookmarkModel.getMobileFolderId())
                || Objects.equals(id, mBookmarkModel.getAccountMobileFolderId());
    }

    private void sortFolders(List<BookmarkId> folders) {
        folders.sort(Comparator.comparingInt(this::getFolderWeight));
    }

    private int getFolderWeight(BookmarkId id) {
        // Check both local and account folder IDs. We can use the same weight logic if we match
        // either the local or account version of the folder.
        if (Objects.equals(id, mBookmarkModel.getDesktopFolderId())
                || Objects.equals(id, mBookmarkModel.getAccountDesktopFolderId())) {
            return 0;
        } else if (Objects.equals(id, mBookmarkModel.getOtherFolderId())
                || Objects.equals(id, mBookmarkModel.getAccountOtherFolderId())) {
            return 1;
        } else if (Objects.equals(id, mBookmarkModel.getLocalOrSyncableReadingListFolder())
                || Objects.equals(id, mBookmarkModel.getAccountReadingListFolder())) {
            return 2;
        } else if (Objects.equals(id, mBookmarkModel.getMobileFolderId())
                || Objects.equals(id, mBookmarkModel.getAccountMobileFolderId())) {
            return 3;
        }
        return 4;
    }

    private ListItem createFolderItem(BookmarkId id) {
        PropertyModel model =
                new PropertyModel.Builder(BookmarkDesktopNavigationProperties.FOLDER_KEYS)
                        .with(BookmarkDesktopNavigationProperties.BOOKMARK_ID, id)
                        .with(
                                BookmarkDesktopNavigationProperties.TITLE,
                                mBookmarkModel.getBookmarkTitle(id))
                        .with(BookmarkDesktopNavigationProperties.ICON, getFolderIcon(id))
                        .with(
                                BookmarkDesktopNavigationProperties.IS_SELECTED,
                                Objects.equals(id, mCurrentFolderId))
                        .with(
                                BookmarkDesktopNavigationProperties.ON_CLICK_HANDLER,
                                () -> mBookmarkDelegate.openFolder(id))
                        .build();
        return new ListItem(BookmarkDesktopNavigationProperties.NAVIGATION_TYPE_FOLDER, model);
    }

    private ListItem createHeaderItem(String title) {
        PropertyModel model =
                new PropertyModel.Builder(BookmarkDesktopNavigationProperties.HEADER_KEYS)
                        .with(BookmarkDesktopNavigationProperties.HEADER_TITLE, title)
                        .build();
        return new ListItem(BookmarkDesktopNavigationProperties.NAVIGATION_TYPE_HEADER, model);
    }

    private Drawable getFolderIcon(BookmarkId id) {
        int iconRes = R.drawable.ic_folder_outline_24dp;
        if (id.getType() == BookmarkType.READING_LIST) {
            iconRes = R.drawable.ic_reading_list_folder_24dp;
        }
        return AppCompatResources.getDrawable(mContext, iconRes);
    }

    private void updateSelectionHighlight() {
        BookmarkId ancestorId = getTopLevelAncestorId(mCurrentFolderId);
        for (ListItem item : mModelList) {
            if (item.type == BookmarkDesktopNavigationProperties.NAVIGATION_TYPE_FOLDER) {
                BookmarkId id = item.model.get(BookmarkDesktopNavigationProperties.BOOKMARK_ID);
                item.model.set(
                        BookmarkDesktopNavigationProperties.IS_SELECTED,
                        Objects.equals(id, ancestorId));
            }
        }
    }

    private @Nullable BookmarkId getTopLevelAncestorId(@Nullable BookmarkId id) {
        if (id == null) return null;

        BookmarkId current = id;
        while (current != null) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(current);
            if (item == null) return null;

            if (Objects.equals(item.getParentId(), mBookmarkModel.getRootFolderId())) {
                return current;
            }
            current = item.getParentId();
        }
        return null;
    }

    // BookmarkUiObserver implementation
    @Override
    public void onDestroy() {
        destroy();
    }

    @Override
    public void onFolderStateSet(@Nullable BookmarkId folder) {
        mCurrentFolderId = folder;
        updateSelectionHighlight();
    }

    @Override
    public void onUiModeChanged(@BookmarkUiMode int mode) {
        if (mode != BookmarkUiMode.FOLDER) {
            mCurrentFolderId = null;
            updateSelectionHighlight();
        }
    }

    // BookmarkModelObserver implementation
    @Override
    public void bookmarkModelChanged() {
        refreshNavigationList();
    }
}
