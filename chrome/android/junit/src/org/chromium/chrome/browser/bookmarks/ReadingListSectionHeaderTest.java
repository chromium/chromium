// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ReadingListSectionHeader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListSectionHeaderTest {
    @Test
    public void testAddReadingListSectionHeaders() {
        Context context = ContextUtils.getApplicationContext();
        String titleRead = context.getString(org.chromium.chrome.R.string.reading_list_read);
        String titleUnread = context.getString(org.chromium.chrome.R.string.reading_list_unread);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(createReadingListEntry(1, true));
        listItems.add(createReadingListEntry(2, true));
        listItems.add(createReadingListEntry(3, false));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 5, listItems.size());
        assertEquals("Expected unread section header", ViewType.SECTION_HEADER,
                listItems.get(0).getViewType());
        assertEquals("Expected unread title text", titleUnread, listItems.get(0).getHeaderTitle());
        assertEquals("Expected read section header", ViewType.SECTION_HEADER,
                listItems.get(2).getViewType());
        assertEquals("Expected read title text", titleRead, listItems.get(2).getHeaderTitle());
        assertEquals(
                "Expected a different item", 3, listItems.get(1).getBookmarkItem().getId().getId());
        assertEquals(
                "Expected a different item", 1, listItems.get(3).getBookmarkItem().getId().getId());
        assertEquals(
                "Expected a different item", 2, listItems.get(4).getBookmarkItem().getId().getId());
    }

    private BookmarkListEntry createReadingListEntry(long id, boolean read) {
        BookmarkId bookmarkId = new BookmarkId(id, BookmarkType.READING_LIST);
        BookmarkItem bookmarkItem =
                new BookmarkItem(bookmarkId, null, null, false, null, false, false, 0, read);
        return BookmarkListEntry.createBookmarkEntry(bookmarkItem);
    }
}
