// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Unit tests for {@link BookmarkUiState} */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkUiStateTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock public BookmarkModel mBookmarkModel;

    @Before
    public void before() {
        doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
        BookmarkId root = new BookmarkId(0, BookmarkType.NORMAL);
        doReturn(root).when(mBookmarkModel).getDefaultFolderViewLocation();
    }

    @Test
    public void testSimpleStatesAndEquality() {
        BookmarkUiState loadingState = BookmarkUiState.createLoadingState();
        assertEquals(loadingState, BookmarkUiState.createLoadingState());

        BookmarkUiState searchState = BookmarkUiState.createSearchState(/* queryString= */ "");
        assertEquals(searchState, BookmarkUiState.createSearchState(/* queryString= */ ""));
        assertNotEquals(searchState, loadingState);
    }

    @Test
    public void testCreateSearchState() {
        BookmarkUiState emptySearchState = BookmarkUiState.createSearchState("");
        assertEquals(emptySearchState, BookmarkUiState.createSearchState(""));

        BookmarkUiState fooSearchState = BookmarkUiState.createSearchState("foo");
        assertEquals(fooSearchState, BookmarkUiState.createSearchState("foo"));
        assertNotEquals(emptySearchState, fooSearchState);
    }

    @Test
    public void testCreateFolderState() {
        BookmarkUiState rootFolderState =
                BookmarkUiState.createFolderState(
                        mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel);
        assertEquals(
                rootFolderState,
                BookmarkUiState.createFolderState(
                        mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel));

        BookmarkId validFolderId = new BookmarkId(1, BookmarkType.NORMAL);
        BookmarkUiState validFolderState =
                BookmarkUiState.createFolderState(validFolderId, mBookmarkModel);
        assertEquals(
                validFolderState, BookmarkUiState.createFolderState(validFolderId, mBookmarkModel));
        assertNotEquals(validFolderState, rootFolderState);
    }

    @Test
    public void testCreateStateFromUrl() {
        BookmarkUiState rootFolderState =
                BookmarkUiState.createStateFromUrl(UrlConstants.BOOKMARKS_URL, mBookmarkModel);
        assertEquals(
                rootFolderState,
                BookmarkUiState.createFolderState(
                        mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel));

        BookmarkUiState invalidFolderState =
                BookmarkUiState.createStateFromUrl("foo", mBookmarkModel);
        assertEquals(
                invalidFolderState,
                BookmarkUiState.createFolderState(
                        mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel));

        BookmarkId validFolderId = new BookmarkId(5, BookmarkType.NORMAL);
        Uri validFolderUri = BookmarkUiState.createFolderUrl(validFolderId);
        BookmarkUiState validFolderState =
                BookmarkUiState.createStateFromUrl(validFolderUri, mBookmarkModel);
        assertEquals(
                validFolderState, BookmarkUiState.createFolderState(validFolderId, mBookmarkModel));
    }

    @Test
    public void testCreateFolderUrl() {
        Uri rootFolderUri =
                BookmarkUiState.createFolderUrl(mBookmarkModel.getDefaultFolderViewLocation());
        assertTrue(rootFolderUri.toString().startsWith(UrlConstants.BOOKMARKS_FOLDER_URL));
        assertEquals(rootFolderUri.getLastPathSegment(), "0");

        final long id = 5;
        BookmarkId normalFolderId = new BookmarkId(id, BookmarkType.NORMAL);
        Uri normalFolderUri = BookmarkUiState.createFolderUrl(normalFolderId);
        assertTrue(normalFolderUri.toString().startsWith(UrlConstants.BOOKMARKS_FOLDER_URL));
        assertEquals(normalFolderUri.getLastPathSegment(), normalFolderId.toString());

        BookmarkId partnerFolderId = new BookmarkId(id, BookmarkType.PARTNER);
        Uri partnerFolderUri = BookmarkUiState.createFolderUrl(partnerFolderId);
        assertTrue(partnerFolderUri.toString().startsWith(UrlConstants.BOOKMARKS_FOLDER_URL));
        assertEquals(partnerFolderUri.getLastPathSegment(), partnerFolderId.toString());

        BookmarkId readingListFolderId = new BookmarkId(id, BookmarkType.READING_LIST);
        Uri readingListFolderUri = BookmarkUiState.createFolderUrl(readingListFolderId);
        assertTrue(readingListFolderUri.toString().startsWith(UrlConstants.BOOKMARKS_FOLDER_URL));
        assertEquals(readingListFolderUri.getLastPathSegment(), readingListFolderId.toString());
    }

    @Test
    public void testIsValid() {
        BookmarkUiState loadingState = BookmarkUiState.createLoadingState();
        assertTrue(loadingState.isValid(mBookmarkModel));

        BookmarkUiState searchState = BookmarkUiState.createSearchState("");
        assertTrue(searchState.isValid(mBookmarkModel));

        BookmarkUiState folderState =
                BookmarkUiState.createFolderState(
                        mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel);
        assertTrue(folderState.isValid(mBookmarkModel));
        doReturn(false).when(mBookmarkModel).doesBookmarkExist(any());
        assertFalse(folderState.isValid(mBookmarkModel));
    }
}
