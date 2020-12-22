// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ReadingListSectionHeader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class ReadingListSectionHeaderTest {
    @Before
    public void setup() {
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testListWithReadUnreadItems() {
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
        assertEquals("Incorrect histogram value for unread items", 1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.ReadingList.NumberOfUnreadItems", 1));
        assertEquals("Incorrect histogram value for read items", 1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.ReadingList.NumberOfReadItems", 2));
        assertEquals("Incorrect histogram value for read list items", 1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.ReadingList.NumberOfItems", 3));
    }

    @Test
    public void testListWithReadUnreadAndPromoItems() {
        Context context = ContextUtils.getApplicationContext();
        String titleRead = context.getString(org.chromium.chrome.R.string.reading_list_read);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(BookmarkListEntry.createSyncPromoHeader(ViewType.PERSONALIZED_SIGNIN_PROMO));
        listItems.add(createReadingListEntry(1, true));
        listItems.add(createReadingListEntry(2, true));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 4, listItems.size());
        assertEquals("Expected promo section header", ViewType.PERSONALIZED_SIGNIN_PROMO,
                listItems.get(0).getViewType());
        assertEquals("Expected read section header", ViewType.SECTION_HEADER,
                listItems.get(1).getViewType());
        assertEquals("Expected read title text", titleRead, listItems.get(1).getHeaderTitle());
        assertEquals(
                "Expected a different item", 1, listItems.get(2).getBookmarkItem().getId().getId());
        assertEquals(
                "Expected a different item", 2, listItems.get(3).getBookmarkItem().getId().getId());
    }

    @Test
    public void testEmptyList() {
        Context context = ContextUtils.getApplicationContext();
        List<BookmarkListEntry> listItems = new ArrayList<>();
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);
        assertEquals("Incorrect number of items in the adapter", 0, listItems.size());
    }

    @Test
    public void testNoReadUnreadItems() {
        Context context = ContextUtils.getApplicationContext();
        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(BookmarkListEntry.createSyncPromoHeader(ViewType.PERSONALIZED_SIGNIN_PROMO));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 1, listItems.size());
        assertEquals("Expected promo section header", ViewType.PERSONALIZED_SIGNIN_PROMO,
                listItems.get(0).getViewType());
    }

    @Test
    public void testUnreadItemsOnly() {
        Context context = ContextUtils.getApplicationContext();
        String titleUnread = context.getString(org.chromium.chrome.R.string.reading_list_unread);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(createReadingListEntry(1, false));
        listItems.add(createReadingListEntry(2, false));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 3, listItems.size());
        assertEquals(
                "Expected section header", ViewType.SECTION_HEADER, listItems.get(0).getViewType());
        assertEquals("Expected unread title text", titleUnread, listItems.get(0).getHeaderTitle());
    }

    @Test
    public void testReadItemsOnly() {
        Context context = ContextUtils.getApplicationContext();
        String titleRead = context.getString(org.chromium.chrome.R.string.reading_list_read);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(createReadingListEntry(1, true));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 2, listItems.size());
        assertEquals(
                "Expected section header", ViewType.SECTION_HEADER, listItems.get(0).getViewType());
        assertEquals("Expected read title text", titleRead, listItems.get(0).getHeaderTitle());
    }

    private BookmarkListEntry createReadingListEntry(long id, boolean read) {
        BookmarkId bookmarkId = new BookmarkId(id, BookmarkType.READING_LIST);
        BookmarkItem bookmarkItem =
                new BookmarkItem(bookmarkId, null, null, false, null, false, false, 0, read);
        return BookmarkListEntry.createBookmarkEntry(bookmarkItem);
    }
}
