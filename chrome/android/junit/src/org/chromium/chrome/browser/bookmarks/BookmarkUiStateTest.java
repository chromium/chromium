// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

/** Unit tests for {@link BookmarkUiState} */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkUiStateTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
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
        // TODO(https://crbug.com/1465675): Write test case.
    }

    @Test
    public void testCreateStateFromUrl() {
        // TODO(https://crbug.com/1465675): Write test case.
    }

    @Test
    public void testCreateFolderUrl() {
        // TODO(https://crbug.com/1465675): Write test case.
    }

    @Test
    public void testIsValid() {
        // TODO(https://crbug.com/1465675): Write test case.
    }
}
