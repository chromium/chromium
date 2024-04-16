// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.Resources;

import androidx.annotation.DimenRes;
import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.Collections;
import java.util.List;

/**
 * Helper class to manage all the logic and UI behind adding the reading list section headers in the
 * bookmark content UI.
 */
class ReadingListSectionHeader {
    /**
     * Sorts the reading list and adds section headers if the list is a reading list. Noop, if the
     * list isn't a reading list. The layout rows are shown in the following order : 1 - Any promo
     * header 2 - Section header with title "Unread" 3 - Unread reading list articles. 4 - Section
     * header with title "Read" 5 - Read reading list articles.
     *
     * @param listItems The list of bookmark items to be shown in the UI.
     */
    public static void maybeSortAndInsertSectionHeaders(List<BookmarkListEntry> listItems) {
        if (listItems.isEmpty()) return;

        // Compute the first reading list index. The topmost item(s) could be promo headers.
        int readingListStartIndex = 0;
        for (BookmarkListEntry listItem : listItems) {
            boolean isReadingListItem =
                    listItem.getBookmarkItem() != null
                            && listItem.getBookmarkItem().getId().getType()
                                    == BookmarkType.READING_LIST;
            if (isReadingListItem) break;
            readingListStartIndex++;
        }

        // If we have no read/unread elements, exit.
        if (readingListStartIndex == listItems.size()) return;
        sort(listItems, readingListStartIndex);
        recordMetrics(listItems);

        // Always show both read/unread section headers even if we may have only one reading list
        // item.
        listItems.add(readingListStartIndex, createReadingListSectionHeader(/* read= */ false));

        // Search for the first read element, and insert the read section header.
        for (int i = readingListStartIndex + 1; i < listItems.size(); i++) {
            BookmarkListEntry listItem = listItems.get(i);
            assert listItem.getBookmarkItem().getId().getType() == BookmarkType.READING_LIST;
            if (listItem.getBookmarkItem().isRead()) {
                listItems.add(i, createReadingListSectionHeader(/* read= */ true));
                return;
            }
        }

        // If no read reading list items, add a read section header at the end.
        listItems.add(listItems.size(), createReadingListSectionHeader(/* read= */ true));
    }

    /** Sorts the given {@code listItems} to show unread items ahead of read items. */
    private static void sort(List<BookmarkListEntry> listItems, int readingListStartIndex) {
        // TODO(crbug.com/40156540): Sort items by creation time possibly.
        Collections.sort(
                listItems.subList(readingListStartIndex, listItems.size()),
                (lhs, rhs) -> {
                    // Unread items are shown first, then sorted based on creation time.
                    BookmarkItem lhsItem = lhs.getBookmarkItem();
                    BookmarkItem rhsItem = rhs.getBookmarkItem();

                    // Sort by read status first.
                    if (lhsItem.isRead() != rhsItem.isRead()) {
                        return Boolean.compare(lhsItem.isRead(), rhsItem.isRead());
                    }

                    // Sort by creation timestamp descending for items with the same read status.
                    return Long.compare(rhsItem.getDateAdded(), lhsItem.getDateAdded());
                });
    }

    private static BookmarkListEntry createReadingListSectionHeader(boolean read) {
        final @StringRes int titleRes =
                read ? R.string.reading_list_read : R.string.reading_list_unread;
        final @DimenRes int topPaddingRes =
                read ? R.dimen.bookmark_reading_list_section_header_padding_top : Resources.ID_NULL;
        return BookmarkListEntry.createSectionHeader(titleRes, topPaddingRes);
    }

    private static void recordMetrics(List<BookmarkListEntry> listItems) {
        int numUnreadItems = 0;
        int numReadItems = 0;
        for (int i = 0; i < listItems.size(); i++) {
            // Skip the headers.
            if (listItems.get(i).getBookmarkItem() == null) continue;
            if (listItems.get(i).getBookmarkItem().isRead()) {
                numReadItems++;
            } else {
                numUnreadItems++;
            }
        }
        RecordUserAction.record("Android.BookmarkPage.ReadingList.OpenReadingList");
        RecordHistogram.recordCount1MHistogram(
                "Bookmarks.ReadingList.NumberOfReadItems", numReadItems);
        RecordHistogram.recordCount1MHistogram(
                "Bookmarks.ReadingList.NumberOfUnreadItems", numUnreadItems);
        RecordHistogram.recordCount1MHistogram(
                "Bookmarks.ReadingList.NumberOfItems", numReadItems + numUnreadItems);
    }
}
