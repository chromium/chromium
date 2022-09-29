// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.annotation.StringRes;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ReadingListSectionHeader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListSectionHeaderTest {
    private static final int NEWER_CREATION_TIMESTAMP = 2;
    private static final int OLDER_CREATION_TIMESTAMP = 1;

    @Before
    public void setup() {
        UmaRecorderHolder.resetForTesting();
    }

    private BookmarkListEntry createReadingListEntry(long id, boolean read, int dateAdded) {
        BookmarkId bookmarkId = new BookmarkId(id, BookmarkType.READING_LIST);
        BookmarkItem bookmarkItem = new BookmarkItem(
                bookmarkId, null, null, false, null, false, false, dateAdded, read);
        return BookmarkListEntry.createBookmarkEntry(bookmarkItem, /*powerBookmarkMeta=*/null);
    }

    private BookmarkListEntry createReadingListEntry(long id, boolean read) {
        return createReadingListEntry(id, read, /*dateAdded=*/0);
    }

    private void assertSectionHeader(
            BookmarkListEntry entry, @StringRes int titleRes, int paddingTopResId) {
        Context context = ContextUtils.getApplicationContext();
        Assert.assertEquals(ViewType.SECTION_HEADER, entry.getViewType());
        Assert.assertEquals(context.getString(titleRes), entry.getSectionHeaderData().headerTitle);
        if (paddingTopResId > 0) {
            Assert.assertEquals(context.getResources().getDimensionPixelSize(paddingTopResId),
                    entry.getSectionHeaderData().topPadding);
        }
    }

    @Test
    public void testListWithReadUnreadItems() {
        Context context = ContextUtils.getApplicationContext();
        String titleRead = context.getString(R.string.reading_list_read);
        String titleUnread = context.getString(R.string.reading_list_unread);

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
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.ReadingList.NumberOfUnreadItems", 1));
        assertEquals("Incorrect histogram value for read items", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.ReadingList.NumberOfReadItems", 2));
        assertEquals("Incorrect histogram value for read list items", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.ReadingList.NumberOfItems", 3));
    }

    @Test
    public void testListWithReadUnreadAndPromoItems() {
        Context context = ContextUtils.getApplicationContext();
        String titleRead = context.getString(R.string.reading_list_read);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(BookmarkListEntry.createSyncPromoHeader(ViewType.PERSONALIZED_SIGNIN_PROMO));
        listItems.add(createReadingListEntry(1, true));
        listItems.add(createReadingListEntry(2, true));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 5, listItems.size());
        assertEquals("Expected promo section header", ViewType.PERSONALIZED_SIGNIN_PROMO,
                listItems.get(0).getViewType());
        assertSectionHeader(listItems.get(1), R.string.reading_list_unread, 0);
        assertSectionHeader(listItems.get(2), R.string.reading_list_read,
                R.dimen.bookmark_reading_list_section_header_padding_top);
        assertEquals(
                "Expected a different item", 1, listItems.get(3).getBookmarkItem().getId().getId());
        assertEquals(
                "Expected a different item", 2, listItems.get(4).getBookmarkItem().getId().getId());
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
        String titleUnread = context.getString(R.string.reading_list_unread);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(createReadingListEntry(1, false, OLDER_CREATION_TIMESTAMP));
        listItems.add(createReadingListEntry(2, false, NEWER_CREATION_TIMESTAMP));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 4, listItems.size());
        assertSectionHeader(listItems.get(0), R.string.reading_list_unread, 0);
        String msg = "Items should be sorted by creation date, newer items comes first.";
        assertEquals(
                msg, NEWER_CREATION_TIMESTAMP, listItems.get(1).getBookmarkItem().getDateAdded());
        assertEquals(
                msg, OLDER_CREATION_TIMESTAMP, listItems.get(2).getBookmarkItem().getDateAdded());
        assertSectionHeader(listItems.get(3), R.string.reading_list_read,
                R.dimen.bookmark_reading_list_section_header_padding_top);
    }

    @Test
    public void testReadItemsOnly() {
        Context context = ContextUtils.getApplicationContext();
        String titleRead = context.getString(R.string.reading_list_read);

        List<BookmarkListEntry> listItems = new ArrayList<>();
        listItems.add(createReadingListEntry(1, true));
        ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(listItems, context);

        assertEquals("Incorrect number of items in the adapter", 3, listItems.size());
        assertSectionHeader(listItems.get(0), R.string.reading_list_unread, 0);
        assertSectionHeader(listItems.get(1), R.string.reading_list_read,
                R.dimen.bookmark_reading_list_section_header_padding_top);
    }
}
