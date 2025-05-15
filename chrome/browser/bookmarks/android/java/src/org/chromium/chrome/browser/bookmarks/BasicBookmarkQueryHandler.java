// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Simple implementation of {@link BookmarkQueryHandler} that fetches children. */
@NullMarked
public class BasicBookmarkQueryHandler implements BookmarkQueryHandler {
    // TODO(crbug.com/40266584): Support pagination.
    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;

    private final BookmarkModel mBookmarkModel;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final @BookmarkNodeMaskBit int mRootFolderForceVisibleMask;

    /**
     * @param bookmarkModel The underlying source of bookmark data.
     * @param bookmarkUiPrefs Stores display preferences for bookmarks.
     * @param rootFolderForceVisibleMask The bitmask used to force visibility of root folder nodes.
     */
    public BasicBookmarkQueryHandler(
            BookmarkModel bookmarkModel,
            BookmarkUiPrefs bookmarkUiPrefs,
            @BookmarkNodeMaskBit int rootFolderForceVisibleMask) {
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mRootFolderForceVisibleMask = rootFolderForceVisibleMask;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(
            BookmarkId parentId, @Nullable Set<PowerBookmarkType> powerFilter) {
        final List<BookmarkId> childIdList =
                parentId.equals(mBookmarkModel.getRootFolderId())
                        ? mBookmarkModel.getTopLevelFolderIds(mRootFolderForceVisibleMask)
                        : mBookmarkModel.getChildIds(parentId);

        final List<BookmarkListEntry> bookmarkListEntries =
                bookmarkIdListToBookmarkListEntryList(childIdList);
        if (parentId.getType() == BookmarkType.READING_LIST) {
            ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(bookmarkListEntries);
        }

        return bookmarkListEntries;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(
            String query, @Nullable Set<PowerBookmarkType> powerFilter) {
        List<BookmarkId> searchIdList =
                mBookmarkModel.searchBookmarks(query, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        List<BookmarkListEntry> allEntries = bookmarkIdListToBookmarkListEntryList(searchIdList);
        if (powerFilter == null || powerFilter.isEmpty()) {
            return allEntries;
        }
        List<BookmarkListEntry> ret = new ArrayList<>();
        for (BookmarkListEntry entry : allEntries) {
            if (powerFilter.contains(getTypeFromMeta(entry.getPowerBookmarkMeta()))) {
                ret.add(entry);
            }
        }
        return ret;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForFolderSelect(BookmarkId parentId) {
        List<BookmarkId> childIdList =
                parentId.equals(mBookmarkModel.getRootFolderId())
                        ? mBookmarkModel.getTopLevelFolderIds(
                                /* forceVisibleMask= */ BookmarkNodeMaskBit.ALL)
                        : mBookmarkModel.getChildIds(parentId);
        List<BookmarkListEntry> bookmarkListEntries =
                bookmarkIdListToBookmarkListEntryList(childIdList);
        List<BookmarkListEntry> ret = new ArrayList<>();
        for (BookmarkListEntry entry : bookmarkListEntries) {
            if (isFolderEntry(entry) && isValidFolder(entry)) {
                ret.add(entry);
            }
        }
        return ret;
    }

    /** Returns whether the given {@link BookmarkListEntry} is a folder. */
    @Contract("null -> false")
    private boolean isFolderEntry(BookmarkListEntry entry) {
        @Nullable BookmarkItem bookmarkItem = entry.getBookmarkItem();
        return bookmarkItem != null && bookmarkItem.isFolder();
    }

    /**
     * Returns whether the given {@link BookmarkListEntry} is a valid folder to add bookmarks to.
     * All entries passed to this function need to be folders, this is enfored by an assert.
     */
    private boolean isValidFolder(BookmarkListEntry entry) {
        assert isFolderEntry(entry);

        // entry.getBookmarkItem() should be null (see isFolderEntry call above).
        BookmarkId folderId = assertNonNull(entry.getBookmarkItem()).getId();
        return BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, folderId);
    }

    private List<BookmarkListEntry> bookmarkIdListToBookmarkListEntryList(
            List<BookmarkId> bookmarkIds) {
        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : bookmarkIds) {
            PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
            if (bookmarkItem == null) continue;
            BookmarkListEntry bookmarkListEntry =
                    BookmarkListEntry.createBookmarkEntry(
                            bookmarkItem,
                            powerBookmarkMeta,
                            mBookmarkUiPrefs.getBookmarkRowDisplayPref());
            bookmarkListEntries.add(bookmarkListEntry);
        }
        return bookmarkListEntries;
    }

    private PowerBookmarkType getTypeFromMeta(@Nullable PowerBookmarkMeta powerBookmarkMeta) {
        if (powerBookmarkMeta != null && powerBookmarkMeta.hasShoppingSpecifics()) {
            return PowerBookmarkType.SHOPPING;
        } else {
            return PowerBookmarkType.UNKNOWN;
        }
    }
}
