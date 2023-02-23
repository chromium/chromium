// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Unit test for {@link ReadingListUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadingListUtilsUnitTest {
    @Mock
    BookmarkModel mBookmarkModel;

    @Mock
    BookmarkId mBookmarkId;
    @Mock
    BookmarkItem mBookmarkItem;
    @Mock
    BookmarkId mReadingListId;
    @Mock
    BookmarkItem mReadingListItem;
    @Mock
    BookmarkId mReadingListFolder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mReadingListId).when(mReadingListItem).getId();
        doReturn(mReadingListItem).when(mBookmarkModel).getReadingListItem(any());
        doAnswer((invocation) -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(mBookmarkModel)
                .finishLoadingBookmarkModel(any());
        doReturn(BookmarkType.NORMAL).when(mBookmarkId).getType();
        doReturn(BookmarkType.READING_LIST).when(mReadingListFolder).getType();
        doReturn(mReadingListFolder).when(mBookmarkModel).getReadingListFolder();
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(mReadingListId).when(mBookmarkModel).addToReadingList(any(), any());
    }

    @Test
    @SmallTest
    public void isReadingListSupported() {
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(null));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(GURL.emptyGURL()));
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL)));
        Assert.assertTrue(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.HTTP_URL)));

        // empty url
        GURL testUrl = GURL.emptyGURL();
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(testUrl));

        // invalid url
        Assert.assertFalse(ReadingListUtils.isReadingListSupported(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.INVALID_URL)));
    }

    @Test
    @SmallTest
    public void deleteFromReadingList() {
        BookmarkModel bookmarkModel = Mockito.mock(BookmarkModel.class);
        BookmarkId readingListId = Mockito.mock(BookmarkId.class);
        BookmarkItem readingListItem = Mockito.mock(BookmarkItem.class);
        doReturn(readingListId).when(readingListItem).getId();
        doReturn(readingListItem).when(bookmarkModel).getReadingListItem(any());
        doAnswer((invocation) -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(bookmarkModel)
                .finishLoadingBookmarkModel(any());

        ReadingListUtils.deleteFromReadingList(bookmarkModel, Mockito.mock(SnackbarManager.class),
                Mockito.mock(Activity.class), Mockito.mock(Tab.class));
        verify(bookmarkModel).getReadingListItem(any());
        verify(bookmarkModel).deleteBookmarks(readingListId);
    }

    @Test
    @SmallTest
    public void isSwappableReadingListItem() {
        BookmarkId readingListId = new BookmarkId(1, BookmarkType.READING_LIST);
        BookmarkId regularId = new BookmarkId(1, BookmarkType.NORMAL);

        Assert.assertFalse(ReadingListUtils.isSwappableReadingListItem(regularId));
        Assert.assertTrue(ReadingListUtils.isSwappableReadingListItem(readingListId));
    }

    @Test
    @SmallTest
    public void maybeTypeSwapAndShowSaveFlow_EdgeCases() {
        BookmarkId bookmarkId = Mockito.mock(BookmarkId.class);
        doReturn(BookmarkType.NORMAL).when(bookmarkId).getType();

        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), /*bookmarkId=*/null, BookmarkType.READING_LIST));

        doReturn(BookmarkType.READING_LIST).when(bookmarkId).getType();
        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), bookmarkId, BookmarkType.READING_LIST));

        doReturn(BookmarkType.NORMAL).when(bookmarkId).getType();
        Assert.assertFalse(ReadingListUtils.maybeTypeSwapAndShowSaveFlow(
                Mockito.mock(Activity.class), Mockito.mock(BottomSheetController.class),
                Mockito.mock(BookmarkModel.class), bookmarkId, BookmarkType.NORMAL));
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToReadingList() {
        BookmarkId parentId = new BookmarkId(0, BookmarkType.READING_LIST);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkModel bookmarkModel = Mockito.mock(BookmarkModel.class);
        doReturn(existingBookmark).when(bookmarkModel).getBookmarkById(existingBookmarkId);
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.READING_LIST);
        doReturn(newBookmarkId)
                .when(bookmarkModel)
                .addToReadingList("Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkModel, bookmarks, typeSwappedBookmarks, parentId);
        verify(bookmarkModel)
                .addToReadingList("Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkModel).deleteBookmark(existingBookmarkId);
        Assert.assertEquals(0, bookmarks.size());
        Assert.assertEquals(1, typeSwappedBookmarks.size());
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToBookmark() {
        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkModel bookmarkModel = Mockito.mock(BookmarkModel.class);
        doReturn(existingBookmark).when(bookmarkModel).getBookmarkById(existingBookmarkId);
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        doReturn(newBookmarkId)
                .when(bookmarkModel)
                .addBookmark(parentId, 0, "Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkModel, bookmarks, typeSwappedBookmarks, parentId);
        verify(bookmarkModel)
                .addBookmark(parentId, 0, "Test", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkModel).deleteBookmark(existingBookmarkId);
        Assert.assertEquals(0, bookmarks.size());
        Assert.assertEquals(1, typeSwappedBookmarks.size());
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_ToBookmark_Multiple() {
        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId1 = new BookmarkId(1, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark1 = new BookmarkItem(existingBookmarkId1, "Test1",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkId existingBookmarkId2 = new BookmarkId(2, BookmarkType.READING_LIST);
        BookmarkItem existingBookmark2 = new BookmarkItem(existingBookmarkId2, "Test2",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkModel bookmarkModel = Mockito.mock(BookmarkModel.class);
        doReturn(existingBookmark1).when(bookmarkModel).getBookmarkById(existingBookmarkId1);
        doReturn(existingBookmark2).when(bookmarkModel).getBookmarkById(existingBookmarkId2);

        BookmarkId newBookmarkId1 = new BookmarkId(3, BookmarkType.NORMAL);
        doReturn(newBookmarkId1)
                .when(bookmarkModel)
                .addBookmark(parentId, 0, "Test1", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        BookmarkId newBookmarkId2 = new BookmarkId(4, BookmarkType.NORMAL);
        doReturn(newBookmarkId2)
                .when(bookmarkModel)
                .addBookmark(parentId, 0, "Test2", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId1);
        bookmarks.add(existingBookmarkId2);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkModel, bookmarks, typeSwappedBookmarks, parentId);
        Assert.assertEquals(0, bookmarks.size());
        Assert.assertEquals(2, typeSwappedBookmarks.size());

        verify(bookmarkModel)
                .addBookmark(parentId, 0, "Test1", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkModel).deleteBookmark(existingBookmarkId1);
        verify(bookmarkModel)
                .addBookmark(parentId, 0, "Test2", JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        verify(bookmarkModel).deleteBookmark(existingBookmarkId2);
    }

    @Test
    @SmallTest
    public void testTypeSwapBookmarksIfNecessary_TypeMatches() {
        BookmarkId parentId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkId existingBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem existingBookmark = new BookmarkItem(existingBookmarkId, "Test",
                JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), /*isFolder=*/false, /*parent=*/null,
                /*isEditable=*/true, /*isManaged=*/false, /*dateAdded*/ 0, /*read=*/false);
        BookmarkModel bookmarkModel = Mockito.mock(BookmarkModel.class);

        ArrayList<BookmarkId> bookmarks = new ArrayList<>();
        bookmarks.add(existingBookmarkId);
        ArrayList<BookmarkId> typeSwappedBookmarks = new ArrayList<>();
        ReadingListUtils.typeSwapBookmarksIfNecessary(
                bookmarkModel, bookmarks, typeSwappedBookmarks, parentId);
        Assert.assertEquals(1, bookmarks.size());
        Assert.assertEquals(0, typeSwappedBookmarks.size());
        Assert.assertEquals(existingBookmarkId, bookmarks.get(0));
    }
}
