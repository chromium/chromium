// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.Collections;
import java.util.List;

/**
 * Helper class to manage all the logic and UI behind adding the reading list section headers in the
 * bookmark content UI.
 * TODO(crbug/1147787): Add integration tests.
 */
class ReadingListSectionHeader {
    /**
     * Sorts the reading list and adds section headers if the list is a reading list.
     * Noop, if the list isn't a reading list. The layout rows are shown in the following order :
     * 1 - Section header with title "Unread"
     * 2 - Unread reading list articles.
     * 3 - Section header with title "Read"
     * 4 - Read reading list articles.
     * @param listItems The list of bookmark items to be shown in the UI.
     * @param context The associated activity context.
     */
    public static void maybeSortAndInsertSectionHeaders(
            List<BookmarkListEntry> listItems, Context context) {
        if (listItems.isEmpty()) return;
        sort(listItems);

        // Add a section header at the top.
        assert listItems.get(0).getBookmarkItem().getId().getType() == BookmarkType.READING_LIST;
        boolean isRead = listItems.get(0).getBookmarkItem().isRead();
        listItems.add(0, createReadingListSectionHeader(isRead, context));
        if (isRead) return;

        // Search for the first read element, and insert the read section header.
        for (int i = 2; i < listItems.size(); i++) {
            BookmarkListEntry listItem = listItems.get(i);
            assert listItem.getBookmarkItem().getId().getType() == BookmarkType.READING_LIST;
            if (listItem.getBookmarkItem().isRead()) {
                listItems.add(i, createReadingListSectionHeader(true /*read*/, context));
                return;
            }
        }
    }

    /**
     * Sorts the given {@code listItems} to show unread items ahead of read items.
     */
    private static void sort(List<BookmarkListEntry> listItems) {
        // TODO(crbug.com/1147259): Sort items by creation time possibly.
        Collections.sort(listItems, (lhs, rhs) -> {
            // Unread items are shown first.
            boolean lhsRead = lhs.getBookmarkItem().isRead();
            boolean rhsRead = rhs.getBookmarkItem().isRead();
            if (lhsRead == rhsRead) return 0;
            return lhsRead ? 1 : -1;
        });
    }

    private static BookmarkListEntry createReadingListSectionHeader(boolean read, Context context) {
        return BookmarkListEntry.createSectionHeader(
                read ? R.string.reading_list_read : R.string.reading_list_unread,
                read ? R.string.reading_list_ready_for_offline : null, context);
    }
}
