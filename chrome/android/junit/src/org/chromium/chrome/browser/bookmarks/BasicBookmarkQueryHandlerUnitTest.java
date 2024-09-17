// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.url.GURL;

import java.util.List;

/** Unit tests for {@link BasicBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BasicBookmarkQueryHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FakeBookmarkModel mBookmarkModel;
    private BasicBookmarkQueryHandler mHandler;

    @Mock ShoppingService mShoppingService;

    @Before
    public void setup() {
        mBookmarkModel = FakeBookmarkModel.createModel();
        mHandler =
                new BasicBookmarkQueryHandler(
                        mBookmarkModel, Mockito.mock(BookmarkUiPrefs.class), mShoppingService);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder() {
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(mBookmarkModel.getRootFolderId());
        assertEquals(4, result.size());
        assertEquals(mBookmarkModel.getOtherFolderId(), result.get(0).getBookmarkItem().getId());
        assertEquals(mBookmarkModel.getDesktopFolderId(), result.get(1).getBookmarkItem().getId());
        assertEquals(mBookmarkModel.getMobileFolderId(), result.get(2).getBookmarkItem().getId());
        assertEquals(
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                result.get(3).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_nonRootFolder() {
        BookmarkId id1 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "test1",
                        new GURL("https://test1.com"));
        BookmarkId id2 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "test2",
                        new GURL("https://test2.com"));
        BookmarkId id3 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "test3",
                        new GURL("https://test3.com"));
        BookmarkId id4 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "test4",
                        new GURL("https://test4.com"));
        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(MOBILE_BOOKMARK_ID);

        assertEquals(5, result.size());
        assertEquals(mBookmarkModel.getPartnerFolderId(), result.get(0).getBookmarkItem().getId());
        assertEquals(id1, result.get(1).getBookmarkItem().getId());
        assertEquals(id2, result.get(2).getBookmarkItem().getId());
        assertEquals(id3, result.get(3).getBookmarkItem().getId());
        assertEquals(id4, result.get(4).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForParent_readingList() {
        BookmarkId readId =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        "test",
                        new GURL("https://test.com"));
        mBookmarkModel.setReadStatusForReadingList(readId, /* read= */ true);
        BookmarkId unreadId =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        "test1",
                        new GURL("https://test1.com"));
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder());

        assertEquals(4, result.size());
        assertEquals(ViewType.SECTION_HEADER, result.get(0).getViewType());
        assertEquals(unreadId, result.get(1).getBookmarkItem().getId());
        assertEquals(ViewType.SECTION_HEADER, result.get(2).getViewType());
        assertEquals(readId, result.get(3).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForSearch() {
        BookmarkId foo1 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "foo1",
                        new GURL("https://foo1.com"));
        BookmarkId foo2 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "foo2",
                        new GURL("https://foo2.com"));
        BookmarkId baz1 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "baz1",
                        new GURL("https://bar1.com"));
        BookmarkId baz2 =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getMobileFolderId(),
                        0,
                        "baz2",
                        new GURL("https://bar2.com"));
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForSearch("foo", /* powerFilter= */ null);
        assertEquals(2, result.size());
        assertEquals(foo1, result.get(0).getBookmarkItem().getId());
        assertEquals(foo2, result.get(1).getBookmarkItem().getId());

        result = mHandler.buildBookmarkListForSearch("baz", /* powerFilter= */ null);
        assertEquals(2, result.size());
        assertEquals(baz1, result.get(0).getBookmarkItem().getId());
        assertEquals(baz2, result.get(1).getBookmarkItem().getId());

        result = mHandler.buildBookmarkListForSearch("fdsa", /* powerFilter= */ null);
        assertEquals(0, result.size());
    }

    @Test
    public void testBuildBookmarkListForFolderSelect() {
        mBookmarkModel.addBookmark(
                mBookmarkModel.getMobileFolderId(), 0, "test1", new GURL("https://test1.com"));
        BookmarkId folder1 =
                mBookmarkModel.addFolder(mBookmarkModel.getMobileFolderId(), 0, "folder 1");
        BookmarkId folder2 =
                mBookmarkModel.addFolder(mBookmarkModel.getMobileFolderId(), 0, "folder 2");
        BookmarkId folder3 =
                mBookmarkModel.addFolder(mBookmarkModel.getMobileFolderId(), 0, "folder 3");

        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForFolderSelect(mBookmarkModel.getMobileFolderId());
        assertEquals(3, result.size());
        assertEquals(folder1, result.get(0).getBookmarkItem().getId());
        assertEquals(folder2, result.get(1).getBookmarkItem().getId());
        assertEquals(folder3, result.get(2).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForFolderSelect_rootFolder() {
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForFolderSelect(mBookmarkModel.getRootFolderId());
        assertEquals(4, result.size());
        assertEquals(mBookmarkModel.getOtherFolderId(), result.get(0).getBookmarkItem().getId());
        assertEquals(mBookmarkModel.getDesktopFolderId(), result.get(1).getBookmarkItem().getId());
        assertEquals(mBookmarkModel.getMobileFolderId(), result.get(2).getBookmarkItem().getId());
        assertEquals(
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                result.get(3).getBookmarkItem().getId());
    }
}
