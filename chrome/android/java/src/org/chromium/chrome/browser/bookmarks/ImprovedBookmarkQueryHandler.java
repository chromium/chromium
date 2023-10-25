// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/** New implementation of {@link BookmarkQueryHandler} that expands the root. */
public class ImprovedBookmarkQueryHandler implements BookmarkQueryHandler {
    private final BookmarkModel mBookmarkModel;
    private final BasicBookmarkQueryHandler mBasicBookmarkQueryHandler;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final ShoppingService mShoppingService;

    /**
     * Constructs a handle that operates on the given backend.
     *
     * @param bookmarkModel The backend that holds the truth of what the bookmark state looks like.
     * @param bookmarkUiPrefs Stores the display prefs for bookmarks.
     * @param shoppingService Supports queries about shopping data.
     */
    public ImprovedBookmarkQueryHandler(
            BookmarkModel bookmarkModel,
            BookmarkUiPrefs bookmarkUiPrefs,
            ShoppingService shoppingService) {
        mBookmarkModel = bookmarkModel;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mShoppingService = shoppingService;
        mBasicBookmarkQueryHandler = new BasicBookmarkQueryHandler(bookmarkModel, mBookmarkUiPrefs);
    }

    @Override
    public void destroy() {
        mBasicBookmarkQueryHandler.destroy();
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForParent(
            BookmarkId parentId, Set<PowerBookmarkType> powerFilter) {
        boolean isReadingList = Objects.equals(parentId, mBookmarkModel.getReadingListFolder());
        final List<BookmarkListEntry> bookmarkListEntries;
        if (!isReadingList && powerFilter != null && !powerFilter.isEmpty()) {
            bookmarkListEntries = collectLeafNodes(parentId);
        } else {
            bookmarkListEntries =
                    mBasicBookmarkQueryHandler.buildBookmarkListForParent(parentId, powerFilter);
        }

        // Don't do anything for ReadingList, they're already sorted with a different mechanism.
        if (!isReadingList) {
            applyPowerFilters(bookmarkListEntries, powerFilter);
            sortByStoredPref(bookmarkListEntries);
        }

        return bookmarkListEntries;
    }

    @Override
    public List<BookmarkListEntry> buildBookmarkListForSearch(
            String query, Set<PowerBookmarkType> powerFilter) {
        if (TextUtils.isEmpty(query)) return Collections.emptyList();
        List<BookmarkListEntry> bookmarkListEntries =
                mBasicBookmarkQueryHandler.buildBookmarkListForSearch(query, powerFilter);
        applyPowerFilters(bookmarkListEntries, powerFilter);
        sortByStoredPref(bookmarkListEntries);
        return bookmarkListEntries;
    }

    private void sortByStoredPref(List<BookmarkListEntry> bookmarkListEntries) {
        final @BookmarkRowSortOrder int sortOrder = mBookmarkUiPrefs.getBookmarkRowSortOrder();
        if (sortOrder == BookmarkRowSortOrder.MANUAL) return;

        Collections.sort(
                bookmarkListEntries,
                (BookmarkListEntry entry1, BookmarkListEntry entry2) -> {
                    BookmarkItem item1 = entry1.getBookmarkItem();
                    BookmarkItem item2 = entry2.getBookmarkItem();

                    // Sort folders before urls.
                    int folderComparison = Boolean.compare(item2.isFolder(), item1.isFolder());
                    if (folderComparison != 0) {
                        return folderComparison;
                    }

                    int titleComparison = sortCompare(item1, item2, sortOrder);
                    if (titleComparison != 0) {
                        return titleComparison;
                    }

                    // Fall back to id in case other fields tie. Order will be arbitrary but
                    // consistent.
                    return Long.compare(item1.getId().getId(), item2.getId().getId());
                });
    }

    private int sortCompare(
            BookmarkItem item1, BookmarkItem item2, @BookmarkRowSortOrder int sortOrder) {
        switch (sortOrder) {
            case BookmarkRowSortOrder.CHRONOLOGICAL:
                return Long.compare(item1.getDateAdded(), item2.getDateAdded());
            case BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL:
                return Long.compare(item2.getDateAdded(), item1.getDateAdded());
            case BookmarkRowSortOrder.ALPHABETICAL:
                return item1.getTitle().compareToIgnoreCase(item2.getTitle());
            case BookmarkRowSortOrder.REVERSE_ALPHABETICAL:
                return item2.getTitle().compareToIgnoreCase(item1.getTitle());
            case BookmarkRowSortOrder.RECENTLY_USED:
                return Long.compare(item2.getDateLastOpened(), item1.getDateLastOpened());
        }
        return 0;
    }

    private void applyPowerFilters(
            List<BookmarkListEntry> bookmarkListEntries,
            @Nullable Set<PowerBookmarkType> powerFilter) {
        if (powerFilter == null || powerFilter.isEmpty()) return;

        // Remove entries from the list if the any filter from powerFilter doesn't match.
        bookmarkListEntries.removeIf(
                bookmarkListEntry -> {
                    // Remove bookmarks which aren't price-tracked if the shopping filter is active.
                    if (powerFilter.contains(PowerBookmarkType.SHOPPING)) {
                        if (!isPriceTracked(bookmarkListEntry)) return true;
                    }

                    return false;
                });
    }

    private boolean isPriceTracked(BookmarkListEntry bookmarkListEntry) {
        PowerBookmarkMeta meta = bookmarkListEntry.getPowerBookmarkMeta();
        if (!PowerBookmarkUtils.isShoppingListItem(meta)) return false;
        return mShoppingService.isSubscribedFromCache(
                PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                        meta.getShoppingSpecifics()));
    }

    private List<BookmarkListEntry> collectLeafNodes(BookmarkId parentId) {
        List<BookmarkListEntry> bookmarkListEntries = new ArrayList<>();
        collectLeafNodesImpl(parentId, bookmarkListEntries);
        return bookmarkListEntries;
    }

    private void collectLeafNodesImpl(
            BookmarkId parentId, List<BookmarkListEntry> bookmarkListEntries) {
        if (parentId == null) return;
        for (BookmarkId childId : mBookmarkModel.getChildIds(parentId)) {
            BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(childId);
            if (bookmarkItem == null) continue;
            if (bookmarkItem.isFolder()) {
                collectLeafNodesImpl(childId, bookmarkListEntries);
            } else {
                bookmarkListEntries.add(
                        BookmarkListEntry.createBookmarkEntry(
                                bookmarkItem,
                                mBookmarkModel.getPowerBookmarkMeta(childId),
                                mBookmarkUiPrefs.getBookmarkRowDisplayPref()));
            }
        }
    }
}
