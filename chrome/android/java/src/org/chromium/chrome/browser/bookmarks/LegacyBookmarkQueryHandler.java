// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;

import java.util.ArrayList;
import java.util.List;

/** Simple implementation of {@link BookmarkQueryHandler} for the old experience. */
public class LegacyBookmarkQueryHandler implements BookmarkQueryHandler {
    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;

    private final BookmarkModel mBookmarkModel;
    private final SyncService mSyncService;
    private final SyncStateChangedListener mSyncStateChangedListener = this::syncStateChanged;
    private final List<BookmarkId> mTopLevelFolders = new ArrayList<>();

    /**
     * @param bookmarkModel The underlying source of bookmark data.
     */
    public LegacyBookmarkQueryHandler(BookmarkModel bookmarkModel) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.finishLoadingBookmarkModel(this::onBookmarkModelLoaded);
        mSyncService = SyncService.get();
        mSyncService.addSyncStateChangedListener(mSyncStateChangedListener);
    }

    @Override
    public void destroy() {
        mSyncService.removeSyncStateChangedListener(mSyncStateChangedListener);
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(BookmarkId parentId) {
        final List<BookmarkId> childIdList;
        if (parentId.equals(mBookmarkModel.getRootFolderId())) {
            childIdList = mTopLevelFolders;
        } else {
            childIdList = mBookmarkModel.getChildIds(parentId);
        }

        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : childIdList) {
            PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
            if (BookmarkId.SHOPPING_FOLDER.equals(parentId)) {
                // TODO(https://crbug.com/1435518): Stop using deprecated #getIsPriceTracked().
                if (powerBookmarkMeta == null || !powerBookmarkMeta.hasShoppingSpecifics()
                        || !powerBookmarkMeta.getShoppingSpecifics().getIsPriceTracked()) {
                    continue;
                }
            }

            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
            BookmarkListEntry bookmarkListEntry =
                    BookmarkListEntry.createBookmarkEntry(bookmarkItem, powerBookmarkMeta);
            bookmarkListEntries.add(bookmarkListEntry);
        }

        if (parentId.getType() == BookmarkType.READING_LIST) {
            ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(bookmarkListEntries);
        }

        return bookmarkListEntries;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(String query) {
        final List<BookmarkId> searchIdList =
                mBookmarkModel.searchBookmarks(query, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : searchIdList) {
            org.chromium.components.power_bookmarks.PowerBookmarkMeta powerBookmarkMeta =
                    mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
            BookmarkListEntry bookmarkListEntry =
                    BookmarkListEntry.createBookmarkEntry(bookmarkItem, powerBookmarkMeta);
            bookmarkListEntries.add(bookmarkListEntry);
        }
        return bookmarkListEntries;
    }

    private void onBookmarkModelLoaded() {
        populateTopLevelFoldersList();
    }

    private void syncStateChanged() {
        if (mBookmarkModel.isBookmarkModelLoaded()) {
            populateTopLevelFoldersList();
        }
    }

    private void populateTopLevelFoldersList() {
        mTopLevelFolders.clear();
        mTopLevelFolders.addAll(BookmarkUtils.populateTopLevelFolders(mBookmarkModel));
    }
}
