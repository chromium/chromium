// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.OTHER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.PARTNER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_ITEM_D;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link BookmarkUtils}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkUtilsTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private BookmarkModel mBookmarkModel;

    @Before
    public void setup() {
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
    }

    @Test
    public void testCanAddFolderWhileViewingParent() {
        assertFalse(BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(
                BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(
                BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertFalse(BookmarkUtils.canAddFolderWhileViewingParent(
                mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(
                BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, PARTNER_BOOKMARK_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testCanAddFolderWhileViewingParent_improvedBookmarksEnabled() {
        assertTrue(BookmarkUtils.canAddFolderWhileViewingParent(mBookmarkModel, ROOT_BOOKMARK_ID));
    }

    @Test
    public void testCanAddBookmarkWhileViewingParent() {
        assertFalse(
                BookmarkUtils.canAddBookmarkWhileViewingParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertTrue(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(BookmarkUtils.canAddBookmarkWhileViewingParent(
                mBookmarkModel, PARTNER_BOOKMARK_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testCanAddBookmarkWhileViewingParent_improvedBookmarksEnabled() {
        assertTrue(
                BookmarkUtils.canAddBookmarkWhileViewingParent(mBookmarkModel, ROOT_BOOKMARK_ID));
    }

    @Test
    public void testMoveBookmarkToViewedParent() {
        List<BookmarkId> bookmarksToMove = new ArrayList<>(Arrays.asList(URL_BOOKMARK_ID_A));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        verify(mBookmarkModel).moveBookmarks(bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        assertEquals(1, bookmarksToMove.size());
        assertEquals(URL_BOOKMARK_ID_A, bookmarksToMove.get(0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testMoveBookmarkToViewedParent_improvedBookmarksEnabled() {
        List<BookmarkId> bookmarksToMove = new ArrayList<>(Arrays.asList(URL_BOOKMARK_ID_A));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        verify(mBookmarkModel).moveBookmarks(bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        assertEquals(1, bookmarksToMove.size());
        assertEquals(URL_BOOKMARK_ID_A, bookmarksToMove.get(0));
    }

    @Test
    public void testMoveBookmarkToViewedParent_Folder() {
        List<BookmarkId> bookmarksToMove = new ArrayList<>(Arrays.asList(FOLDER_BOOKMARK_ID_A));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, MOBILE_BOOKMARK_ID);
        verify(mBookmarkModel).moveBookmarks(bookmarksToMove, MOBILE_BOOKMARK_ID);
        assertEquals(1, bookmarksToMove.size());
        assertEquals(FOLDER_BOOKMARK_ID_A, bookmarksToMove.get(0));
    }

    @Test
    public void testMoveBookmarkToViewedParent_toRootFolder() {
        List<BookmarkId> bookmarksToMove = new ArrayList<>(Arrays.asList(URL_BOOKMARK_ID_A));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, ROOT_BOOKMARK_ID);
        verify(mBookmarkModel, times(0)).moveBookmarks(bookmarksToMove, ROOT_BOOKMARK_ID);
        assertEquals(1, bookmarksToMove.size());
        assertEquals(URL_BOOKMARK_ID_A, bookmarksToMove.get(0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testMoveBookmarkToViewedParent_toRootFolder_improvedBookmarksEnabled() {
        List<BookmarkId> bookmarksToMove = new ArrayList<>(Arrays.asList(URL_BOOKMARK_ID_A));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, ROOT_BOOKMARK_ID);
        verify(mBookmarkModel).moveBookmarks(bookmarksToMove, OTHER_BOOKMARK_ID);
        assertEquals(1, bookmarksToMove.size());
        assertEquals(URL_BOOKMARK_ID_A, bookmarksToMove.get(0));
    }

    @Test
    public void testMoveBookmarkToViewedParent_readingList() {
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        doReturn(newBookmarkId)
                .when(mBookmarkModel)
                .addBookmark(FOLDER_BOOKMARK_ID_A, 0, URL_ITEM_D.getTitle(), URL_ITEM_D.getUrl());

        List<BookmarkId> bookmarksToMove = new ArrayList<>(Arrays.asList(URL_BOOKMARK_ID_D));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        verify(mBookmarkModel)
                .addBookmark(FOLDER_BOOKMARK_ID_A, 0, URL_ITEM_D.getTitle(), URL_ITEM_D.getUrl());
        verify(mBookmarkModel).moveBookmarks(bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        assertEquals(1, bookmarksToMove.size());
        assertEquals(newBookmarkId, bookmarksToMove.get(0));
    }

    @Test
    public void testMoveBookmarkToViewedParent_readingListAndBookmark() {
        BookmarkId newBookmarkId = new BookmarkId(0, BookmarkType.NORMAL);
        doReturn(newBookmarkId)
                .when(mBookmarkModel)
                .addBookmark(FOLDER_BOOKMARK_ID_A, 0, URL_ITEM_D.getTitle(), URL_ITEM_D.getUrl());

        List<BookmarkId> bookmarksToMove =
                new ArrayList<>(Arrays.asList(URL_BOOKMARK_ID_D, URL_BOOKMARK_ID_A));
        BookmarkUtils.moveBookmarksToViewedParent(
                mBookmarkModel, bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        verify(mBookmarkModel, times(1)).moveBookmarks(bookmarksToMove, FOLDER_BOOKMARK_ID_A);
        assertEquals(2, bookmarksToMove.size());
        assertEquals(URL_BOOKMARK_ID_A, bookmarksToMove.get(0));
        assertEquals(newBookmarkId, bookmarksToMove.get(1));
    }

    @Test
    public void testGetParentFolderForViewing() {
        assertEquals(MOBILE_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertEquals(ROOT_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, OTHER_BOOKMARK_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testGetParentFolderForViewing_improvedBookmarksEnabled() {
        assertEquals(ROOT_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertEquals(ROOT_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, OTHER_BOOKMARK_ID));
    }
}
