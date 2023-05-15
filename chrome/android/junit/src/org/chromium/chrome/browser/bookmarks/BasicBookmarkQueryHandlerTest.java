// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_B;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_E;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link BasicBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BasicBookmarkQueryHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;

    private BasicBookmarkQueryHandler mHandler;

    @Before
    public void setup() {
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
        mHandler = new BasicBookmarkQueryHandler(mBookmarkModel, mBookmarkUiPrefs);
    }

    @Test
    public void testBuildBookmarkListForParent_nonRootFolder() {
        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(MOBILE_BOOKMARK_ID);

        assertEquals(2, result.size());
        assertEquals(FOLDER_BOOKMARK_ID_A, result.get(0).getBookmarkItem().getId());
        assertEquals(URL_BOOKMARK_ID_A, result.get(1).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_shopping() {
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(BookmarkId.SHOPPING_FOLDER);

        // Both URL_BOOKMARK_ID_B and URL_BOOKMARK_ID_C will be returned as children of
        // BookmarkId.SHOPPING_FOLDER , but only URL_BOOKMARK_ID_B will have a correct meta.
        assertEquals(1, result.size());
        assertEquals(URL_BOOKMARK_ID_B, result.get(0).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_readingList() {
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(READING_LIST_BOOKMARK_ID);

        assertEquals(4, result.size());
        // While the getChildIds call will return [D, E], due to the read status, they should get
        // flipped around to show the unread E first. Headers will also be inserted.
        assertEquals(ViewType.SECTION_HEADER, result.get(0).getViewType());
        assertEquals(URL_BOOKMARK_ID_E, result.get(1).getBookmarkItem().getId());
        assertEquals(ViewType.SECTION_HEADER, result.get(2).getViewType());
        assertEquals(URL_BOOKMARK_ID_D, result.get(3).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForSearch() {
        doReturn(Arrays.asList(FOLDER_BOOKMARK_ID_A, URL_BOOKMARK_ID_A))
                .when(mBookmarkModel)
                .searchBookmarks("A", 500);
        List<BookmarkListEntry> result = mHandler.buildBookmarkListForSearch("A");
        assertEquals(2, result.size());
    }
}
