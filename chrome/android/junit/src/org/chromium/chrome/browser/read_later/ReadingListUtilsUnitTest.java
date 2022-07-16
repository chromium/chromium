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
import java.util.List;

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
        List<BookmarkId> newBookmarks =
                ReadingListUtils.typeSwapBookmarksIfNecessary(bookmarkBridge, bookmarks, parentId);
        Assert.assertEquals(bookmarks.size(), newBookmarks.size());
        Assert.assertEquals(bookmarks.get(0), newBookmarks.get(0));
    }
}
