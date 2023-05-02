// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_B;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_E;

import org.junit.Assert;
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

    @Before
    public void setup() {
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
    }

    @Test
    public void testBuildBookmarkListForParent_nonRootFolder() {
        BookmarkQueryHandler bookmarkQueryHandler = new BasicBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(MOBILE_BOOKMARK_ID);

        Assert.assertEquals(2, result.size());
        Assert.assertEquals(FOLDER_BOOKMARK_ID_A, result.get(0).getBookmarkItem().getId());
        Assert.assertEquals(URL_BOOKMARK_ID_A, result.get(1).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_shopping() {
        BookmarkQueryHandler bookmarkQueryHandler = new BasicBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(BookmarkId.SHOPPING_FOLDER);

        // Both URL_BOOKMARK_ID_B and URL_BOOKMARK_ID_C will be returned as children of
        // BookmarkId.SHOPPING_FOLDER , but only URL_BOOKMARK_ID_B will have a correct meta.
        Assert.assertEquals(1, result.size());
        Assert.assertEquals(URL_BOOKMARK_ID_B, result.get(0).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_readingList() {
        BookmarkQueryHandler bookmarkQueryHandler = new BasicBookmarkQueryHandler(mBookmarkModel);
        List<BookmarkListEntry> result =
                bookmarkQueryHandler.buildBookmarkListForParent(READING_LIST_BOOKMARK_ID);

        Assert.assertEquals(4, result.size());
        // While the getChildIds call will return [D, E], due to the read status, they should get
        // flipped around to show the unread E first. Headers will also be inserted.
        Assert.assertEquals(ViewType.SECTION_HEADER, result.get(0).getViewType());
        Assert.assertEquals(URL_BOOKMARK_ID_E, result.get(1).getBookmarkItem().getId());
        Assert.assertEquals(ViewType.SECTION_HEADER, result.get(2).getViewType());
        Assert.assertEquals(URL_BOOKMARK_ID_D, result.get(3).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForSearch() {
        BookmarkQueryHandler bookmarkQueryHandler = new BasicBookmarkQueryHandler(mBookmarkModel);
        doReturn(Arrays.asList(FOLDER_BOOKMARK_ID_A, URL_BOOKMARK_ID_A))
                .when(mBookmarkModel)
                .searchBookmarks("A", 500);
        List<BookmarkListEntry> result = bookmarkQueryHandler.buildBookmarkListForSearch("A");
        Assert.assertEquals(2, result.size());
    }
}
