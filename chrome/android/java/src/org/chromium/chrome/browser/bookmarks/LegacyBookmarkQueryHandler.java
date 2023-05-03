// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;

import java.util.ArrayList;
import java.util.List;

/** Simple implementation of {@link BookmarkQueryHandler} for the old experience. */
public class LegacyBookmarkQueryHandler implements BookmarkQueryHandler {
    private final BasicBookmarkQueryHandler mBasicBookmarkQueryHandler;
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
        mBasicBookmarkQueryHandler = new BasicBookmarkQueryHandler(bookmarkModel);
    }

    @Override
    public void destroy() {
        mSyncService.removeSyncStateChangedListener(mSyncStateChangedListener);
        mBasicBookmarkQueryHandler.destroy();
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(BookmarkId parentId) {
        if (parentId.equals(mBookmarkModel.getRootFolderId())) {
            return buildBookmarkListForRootView();
        } else {
            return mBasicBookmarkQueryHandler.buildBookmarkListForParent(parentId);
        }
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(String query) {
        return mBasicBookmarkQueryHandler.buildBookmarkListForSearch(query);
    }

    private List<BookmarkListEntry> buildBookmarkListForRootView() {
        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : mTopLevelFolders) {
            PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
            BookmarkListEntry bookmarkListEntry =
                    BookmarkListEntry.createBookmarkEntry(bookmarkItem, powerBookmarkMeta);
            bookmarkListEntries.add(bookmarkListEntry);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST)) {
            bookmarkListEntries.add(BookmarkListEntry.createDivider());
            bookmarkListEntries.add(BookmarkListEntry.createShoppingFilter());
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
