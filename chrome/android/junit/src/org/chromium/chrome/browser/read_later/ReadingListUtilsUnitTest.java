// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Unit test for {@link ReadingListUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListUtilsUnitTest {
    @Test
    @SmallTest
    public void testIsReadingListSupport() {
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(null));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(GURL.emptyGURL()));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.HTTP_URL)));
    }

    private void allowBookmarkTypeSwapping() {
        TestValues readLaterTypeSwapping = new TestValues();
        readLaterTypeSwapping.addFeatureFlagOverride(ChromeFeatureList.READ_LATER, true);
        readLaterTypeSwapping.addFieldTrialParamOverride(
                ChromeFeatureList.READ_LATER, "allow_bookmark_type_swapping", "true");
        FeatureList.setTestValues(readLaterTypeSwapping);
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToReadingList() {
        allowBookmarkTypeSwapping();

        BookmarkId parentId = new BookmarkId(0, BookmarkType.READING_LIST);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);
        doReturn(existingBookmark).when(bookmarkBridge).getBookmarkById(existingBookmarkId);
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.READING_LIST);
        doReturn(newBookmarkId)
                .when(bookmarkBridge)
                .addToReadingList("Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ReadingListUtils.typeSwapBookmarksIfNecessary(bookmarkBridge, bookmarks, parentId);
        verify(bookmarkBridge)
                .addToReadingList("Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId);
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToBookmark() {
        allowBookmarkTypeSwapping();

        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);
        doReturn(existingBookmark).when(bookmarkBridge).getBookmarkById(existingBookmarkId);
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        doReturn(newBookmarkId)
                .when(bookmarkBridge)
                .addBookmark(parentId, 0, "Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ReadingListUtils.typeSwapBookmarksIfNecessary(bookmarkBridge, bookmarks, parentId);
        verify(bookmarkBridge)
                .addBookmark(parentId, 0, "Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId);
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToBookmark_Multiple() {
        allowBookmarkTypeSwapping();

        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId1 = new BookmarkId(1, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark1 = new BookmarkItem(existingBookmarkId1, "Test1",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkId existingBookmarkId2 = new BookmarkId(2, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark2 = new BookmarkItem(existingBookmarkId2, "Test2",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);
        doReturn(existingBookmark1).when(bookmarkBridge).getBookmarkById(existingBookmarkId1);
        doReturn(existingBookmark2).when(bookmarkBridge).getBookmarkById(existingBookmarkId2);

        BookmarkId newBookmarkId1 = new BookmarkId(3, BookmarkType.NORMAL);
        doReturn(newBookmarkId1)
                .when(bookmarkBridge)
                .addBookmark(parentId, 0, "Test1", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        BookmarkId newBookmarkId2 = new BookmarkId(4, BookmarkType.NORMAL);
        doReturn(newBookmarkId2)
                .when(bookmarkBridge)
                .addBookmark(parentId, 0, "Test2", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId1);
        bookmarks.add(existingBookmarkId2);
        ReadingListUtils.typeSwapBookmarksIfNecessary(bookmarkBridge, bookmarks, parentId);
        Assert.assertEquals(2, bookmarks.size());

        verify(bookmarkBridge)
                .addBookmark(parentId, 0, "Test1", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId1);
        verify(bookmarkBridge)
                .addBookmark(parentId, 0, "Test2", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkBridge).deleteBookmark(existingBookmarkId2);
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_TypeMatches() {
        allowBookmarkTypeSwapping();

        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkBridge bookmarkBridge = Mockito.mock(BookmarkBridge.class);

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ReadingListUtils.typeSwapBookmarksIfNecessary(bookmarkBridge, bookmarks, parentId);
        Assert.assertEquals(existingBookmarkId, bookmarks.get(0));
    }
}
