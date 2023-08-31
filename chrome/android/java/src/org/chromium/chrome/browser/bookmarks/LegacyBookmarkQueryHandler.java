// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.components.sync.SyncService;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Simple implementation of {@link BookmarkQueryHandler} for the old experience. */
public class LegacyBookmarkQueryHandler implements BookmarkQueryHandler {
    private final BasicBookmarkQueryHandler mBasicBookmarkQueryHandler;
    private final BookmarkModel mBookmarkModel;
    private final SyncService mSyncService;
    private final SyncService.SyncStateChangedListener mSyncStateChangedListener =
            this::syncStateChanged;
    private final List<BookmarkId> mTopLevelFolders = new ArrayList<>();
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    /**
     * @param bookmarkModel The underlying source of bookmark data.
     * @param bookmarkUiPrefs Stores display preferences for bookmarks.
     */
    public LegacyBookmarkQueryHandler(
            BookmarkModel bookmarkModel, BookmarkUiPrefs bookmarkUiPrefs, SyncService syncService) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.finishLoadingBookmarkModel(this::onBookmarkModelLoaded);
        mSyncService = syncService;
        mSyncService.addSyncStateChangedListener(mSyncStateChangedListener);
        mBasicBookmarkQueryHandler = new BasicBookmarkQueryHandler(bookmarkModel, bookmarkUiPrefs);
        mBookmarkUiPrefs = bookmarkUiPrefs;
    }

    @Override
    public void destroy() {
        mSyncService.removeSyncStateChangedListener(mSyncStateChangedListener);
        mBasicBookmarkQueryHandler.destroy();
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(
            BookmarkId parentId, Set<PowerBookmarkType> powerFilter) {
        if (parentId.equals(mBookmarkModel.getRootFolderId())) {
            return buildBookmarkListForRootView();
        } else {
            return mBasicBookmarkQueryHandler.buildBookmarkListForParent(parentId, powerFilter);
        }
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(
            String query, Set<PowerBookmarkType> powerFilter) {
        return mBasicBookmarkQueryHandler.buildBookmarkListForSearch(query, powerFilter);
    }

    private List<BookmarkListEntry> buildBookmarkListForRootView() {
        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : mTopLevelFolders) {
            PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
            BookmarkListEntry bookmarkListEntry = BookmarkListEntry.createBookmarkEntry(
                    bookmarkItem, powerBookmarkMeta, mBookmarkUiPrefs.getBookmarkRowDisplayPref());
            bookmarkListEntries.add(bookmarkListEntry);
        }

        if (ShoppingFeatures.isShoppingListEligible()) {
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
