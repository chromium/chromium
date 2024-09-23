// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.annotation.Nullable;

import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/** Simple implementation of {@link BookmarkQueryHandler} that fetches children. */
public class BasicBookmarkQueryHandler implements BookmarkQueryHandler {
    // TODO(crbug.com/40266584): Support pagination.
    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;

    private final BookmarkModel mBookmarkModel;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final ShoppingService mShoppingService;

    /**
     * @param bookmarkModel The underlying source of bookmark data.
     * @param bookmarkUiPrefs Stores display preferences for bookmarks.
     */
    public BasicBookmarkQueryHandler(
            BookmarkModel bookmarkModel,
            BookmarkUiPrefs bookmarkUiPrefs,
            ShoppingService shoppingService) {
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mShoppingService = shoppingService;
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
    public List<BookmarkListEntry> buildBookmarkListForFolderSelect(BookmarkId parentId) {
        List<BookmarkId> childIdList =
                parentId.equals(mBookmarkModel.getRootFolderId())
                        ? mBookmarkModel.getTopLevelFolderIds(/* ignoreVisibility= */ true)
                        : mBookmarkModel.getChildIds(parentId);
        List<BookmarkListEntry> bookmarkListEntries =
                bookmarkIdListToBookmarkListEntryList(childIdList, parentId);
        bookmarkListEntries =
                bookmarkListEntries.stream()
                        .filter(this::isFolderEntry)
                        .filter(entry -> isValidFolder(entry))
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
     */
    private boolean isValidFolder(BookmarkListEntry entry) {
        assert entry.getBookmarkItem().isFolder();

        BookmarkId folderId = entry.getBookmarkItem().getId();
        return BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, folderId);
    }

    private List<BookmarkListEntry> bookmarkIdListToBookmarkListEntryList(
            List<BookmarkId> bookmarkIds, @Nullable BookmarkId parentId) {
        final List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        for (BookmarkId bookmarkId : bookmarkIds) {
            PowerBookmarkMeta powerBookmarkMeta = mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
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

    private boolean isBookmarkMetaSubscribed(PowerBookmarkMeta powerBookmarkMeta) {
        if (mShoppingService == null
                || powerBookmarkMeta == null
                || !powerBookmarkMeta.hasShoppingSpecifics()
                || !powerBookmarkMeta.getShoppingSpecifics().hasProductClusterId()) {
            return false;
        }

        // TODO(b:326440332): Ideally this uses PriceTrackingUtils.IsBookmarkPriceTracked,
        //                    but the UI does not currently support async updates which is
        //                    required by that api.
        return mShoppingService.isSubscribedFromCache(
                PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(
                        powerBookmarkMeta));
    }
}
