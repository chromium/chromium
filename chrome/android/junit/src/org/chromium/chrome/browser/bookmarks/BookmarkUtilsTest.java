// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.OTHER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.PARTNER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;

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
import org.chromium.base.test.util.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;

/** Unit tests for {@link BookmarkUtils}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock private BookmarkModel mBookmarkModel;

    @Before
    public void setup() {
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
    }

    @Test
    public void testCanAddFolderToParent_accountFolders() {
        BookmarkModel fakeBookmarkModel = FakeBookmarkModel.createModel();
        assertFalse(
                BookmarkUtils.canAddFolderToParent(
                        fakeBookmarkModel, fakeBookmarkModel.getAccountReadingListFolder()));
    }

    @Test
    public void testCanAddFolderToParent() {
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddFolderToParent(mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddFolderToParent(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, PARTNER_BOOKMARK_ID));

        BookmarkId managedBookmarkId = new BookmarkId(123, BookmarkType.NORMAL);
        BookmarkItem managedBookmarkItem =
                new BookmarkItem(
                        managedBookmarkId,
                        "managed",
                        null,
                        true,
                        ROOT_BOOKMARK_ID,
                        false,
                        true,
                        0,
                        false,
                        0,
                        false);
        doReturn(managedBookmarkItem).when(mBookmarkModel).getBookmarkById(managedBookmarkId);
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, managedBookmarkId));
    }

    @Test
    public void testCanAddBookmarkToParent() {
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertTrue(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, PARTNER_BOOKMARK_ID));

        // Null case
        BookmarkId nullBookmarkItemId = new BookmarkId(123, BookmarkType.NORMAL);
        doReturn(null).when(mBookmarkModel).getBookmarkById(nullBookmarkItemId);
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, nullBookmarkItemId));

        BookmarkId managedBookmarkId = new BookmarkId(123, BookmarkType.NORMAL);
        BookmarkItem managedBookmarkItem =
                new BookmarkItem(
                        managedBookmarkId,
                        "managed",
                        null,
                        true,
                        ROOT_BOOKMARK_ID,
                        false,
                        true,
                        0,
                        false,
                        0,
                        false);
        doReturn(managedBookmarkItem).when(mBookmarkModel).getBookmarkById(managedBookmarkId);
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, managedBookmarkId));
    }

    @Test
    public void testGetParentFolderForViewing() {
        assertEquals(
                MOBILE_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertEquals(
                ROOT_BOOKMARK_ID,
                BookmarkUtils.getParentFolderForViewing(mBookmarkModel, OTHER_BOOKMARK_ID));
    }
}
