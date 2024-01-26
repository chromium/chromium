// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.Nullable;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/** Simple implementation of {@link BookmarkQueryHandler} that fetches children. */
public class BasicBookmarkQueryHandler implements BookmarkQueryHandler {
    // TODO(https://crbug.com/1441629): Support pagination.
    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;

    private final BookmarkModel mBookmarkModel;
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    /**
     * @param bookmarkModel The underlying source of bookmark data.
     * @param bookmarkUiPrefs Stores display preferences for bookmarks.
     */
    public BasicBookmarkQueryHandler(BookmarkModel bookmarkModel, BookmarkUiPrefs bookmarkUiPrefs) {
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(
            BookmarkId parentId, Set<PowerBookmarkType> powerFilter) {
        final List<BookmarkId> childIdList =
                parentId.equals(mBookmarkModel.getRootFolderId())
                        ? mBookmarkModel.getTopLevelFolderIds()
                        : mBookmarkModel.getChildIds(parentId);

        final List<BookmarkListEntry> bookmarkListEntries =
                bookmarkIdListToBookmarkListEntryList(childIdList, parentId);
        if (parentId.getType() == BookmarkType.READING_LIST) {
            ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(bookmarkListEntries);
        }

        return bookmarkListEntries;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(
            String query, Set<PowerBookmarkType> powerFilter) {
        final List<BookmarkId> searchIdList =
                mBookmarkModel.searchBookmarks(query, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        final boolean isFilterEmpty = powerFilter == null || powerFilter.isEmpty();
        return bookmarkIdListToBookmarkListEntryList(searchIdList, null).stream()
                .filter(
                        entry -> {
                            return isFilterEmpty
                                    || powerFilter.contains(
                                            getTypeFromMeta(entry.getPowerBookmarkMeta()));
                        })
                .collect(Collectors.toList());
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForFolderSelect(
            BookmarkId parentId, boolean movingFolder) {
        List<BookmarkId> childIdList =
                parentId.equals(mBookmarkModel.getRootFolderId())
                        ? mBookmarkModel.getTopLevelFolderIds(/* ignoreVisibility= */ true)
                        : mBookmarkModel.getChildIds(parentId);
        List<BookmarkListEntry> bookmarkListEntries =
                bookmarkIdListToBookmarkListEntryList(childIdList, parentId);
        bookmarkListEntries =
                bookmarkListEntries.stream()
                        .filter(this::isFolderEntry)
                        .filter(entry -> isValidFolder(entry, movingFolder))
                        .collect(Collectors.toList());
        return bookmarkListEntries;
    }

    /** Returns whether the given {@link BookmarkListEntry} is a folder. */
    private boolean isFolderEntry(BookmarkListEntry entry) {
        return entry.getBookmarkItem().isFolder();
    }

    /**
     * Returns whether the given {@link BookmarkListEntry} is a valid folder to add bookmarks to.
     * All entries passed to this function need to be folders, this is enfored by an assert.
     *
     * @param entry The {@link BookmarkListEntry} to check.
     * @param movingFolder Whether there's a folder being moved. Will change the validity of certain
     *     folders (e.g. reading list).
     * @return Whether the given {@link BookmarkListEntry} is a valid folder to save to.
     */
    private boolean isValidFolder(BookmarkListEntry entry, boolean movingFolder) {
        assert entry.getBookmarkItem().isFolder();

        BookmarkId folderId = entry.getBookmarkItem().getId();
        if (movingFolder) {
            return BookmarkUtils.canAddFolderToParent(mBookmarkModel, folderId);
        }
        return BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, folderId);
    }

    private List<BookmarkListEntry> bookmarkIdListToBookmarkListEntryList(
            List<BookmarkId> bookmarkIds, @Nullable BookmarkId parentId) {
        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : bookmarkIds) {
            PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
            if (BookmarkId.SHOPPING_FOLDER.equals(parentId)) {
                // TODO(https://crbug.com/1435518): Stop using deprecated #getIsPriceTracked().
                if (powerBookmarkMeta == null
                        || !powerBookmarkMeta.hasShoppingSpecifics()
                        || !powerBookmarkMeta.getShoppingSpecifics().getIsPriceTracked()) {
                    continue;
                }
            }
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
            BookmarkListEntry bookmarkListEntry =
                    BookmarkListEntry.createBookmarkEntry(
                            bookmarkItem,
                            powerBookmarkMeta,
                            mBookmarkUiPrefs.getBookmarkRowDisplayPref());
            bookmarkListEntries.add(bookmarkListEntry);
        }
        return bookmarkListEntries;
    }

    private PowerBookmarkType getTypeFromMeta(PowerBookmarkMeta powerBookmarkMeta) {
        if (powerBookmarkMeta != null && powerBookmarkMeta.hasShoppingSpecifics()) {
            return PowerBookmarkType.SHOPPING;
        } else {
            return PowerBookmarkType.UNKNOWN;
        }
    }
}
