// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link FakeBookmarkModel}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FakeBookmarkModelTest {

    private FakeBookmarkModel mBookmarkModel;

    @Before
    public void setup() {
        mBookmarkModel = (FakeBookmarkModel) FakeBookmarkModel.createModel();
    }

    @Test
    public void testDefaultFolders() {
        List<BookmarkId> expected =
                Arrays.asList(
                        mBookmarkModel.getOtherFolderId(),
                        mBookmarkModel.getDesktopFolderId(),
                        mBookmarkModel.getMobileFolderId(),
                        mBookmarkModel.getLocalOrSyncableReadingListFolder());
        assertEquals(expected, mBookmarkModel.getTopLevelFolderIds());

        expected = Arrays.asList(mBookmarkModel.getPartnerFolderId());
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getMobileFolderId()));
    }

    @Test
    public void testDefaultFolders_accountStorageEnabled() {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        List<BookmarkId> expected =
                Arrays.asList(
                        mBookmarkModel.getOtherFolderId(),
                        mBookmarkModel.getDesktopFolderId(),
                        mBookmarkModel.getMobileFolderId(),
                        mBookmarkModel.getAccountOtherFolderId(),
                        mBookmarkModel.getAccountDesktopFolderId(),
                        mBookmarkModel.getAccountMobileFolderId(),
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        mBookmarkModel.getAccountReadingListFolder());
        assertEquals(expected, mBookmarkModel.getTopLevelFolderIds());
        assertTrue(mBookmarkModel.isAccountBookmark(mBookmarkModel.getAccountReadingListFolder()));

        expected = Arrays.asList(mBookmarkModel.getPartnerFolderId());
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getMobileFolderId()));
    }

    @Test
    public void testAddFolder() {
        BookmarkId id =
                mBookmarkModel.addFolder(mBookmarkModel.getOtherFolderId(), 0, "user folder");

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getOtherFolderId()));

        BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        assertNotNull(item);
        assertTrue(item.isFolder());
    }

    @Test
    public void testAddBokmark() {
        BookmarkId id =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getOtherFolderId(),
                        0,
                        "user bookmark",
                        new GURL("https://google.com"));

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getOtherFolderId()));

        BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        assertNotNull(item);
        assertFalse(item.isFolder());
    }

    @Test
    public void testAddAccountReadingListBokmark() {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        BookmarkId id =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getAccountReadingListFolder(),
                        "user account bookmark",
                        new GURL("https://google.com"));

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(
                expected, mBookmarkModel.getChildIds(mBookmarkModel.getAccountReadingListFolder()));
        assertTrue(mBookmarkModel.getBookmarkById(id).isAccountBookmark());
    }

    @Test
    public void testEditBookmark() {
        BookmarkId id =
                mBookmarkModel.addBookmark(
                        mBookmarkModel.getOtherFolderId(),
                        0,
                        "user bookmark",
                        new GURL("https://google.com/"));

        assertEquals("user bookmark", mBookmarkModel.getBookmarkById(id).getTitle());
        mBookmarkModel.setBookmarkTitle(id, "user bookmark 2");
        assertEquals("user bookmark 2", mBookmarkModel.getBookmarkById(id).getTitle());

        assertEquals("https://google.com/", mBookmarkModel.getBookmarkById(id).getUrl().getSpec());
        mBookmarkModel.setBookmarkUrl(id, new GURL("https://google2.com/"));
        assertEquals("https://google2.com/", mBookmarkModel.getBookmarkById(id).getUrl().getSpec());

        assertEquals(
                mBookmarkModel.getOtherFolderId(),
                mBookmarkModel.getBookmarkById(id).getParentId());
        mBookmarkModel.moveBookmark(id, mBookmarkModel.getMobileFolderId(), 0);
        assertEquals(
                mBookmarkModel.getMobileFolderId(),
                mBookmarkModel.getBookmarkById(id).getParentId());
    }

    @Test
    public void testReadingList() {
        BookmarkId id1 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        "rl1",
                        new GURL("https://test1.com"));
        BookmarkId id2 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        "rl2",
                        new GURL("https://test2.com"));
        BookmarkId id3 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        "rl3",
                        new GURL("https://test3.com"));

        List<BookmarkId> expected = Arrays.asList(id1, id2, id3);
        assertEquals(
                expected,
                mBookmarkModel.getChildIds(mBookmarkModel.getLocalOrSyncableReadingListFolder()));
        assertEquals(
                3,
                mBookmarkModel.getUnreadCount(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder()));

        mBookmarkModel.setReadStatusForReadingList(id1, true);
        assertEquals(
                2,
                mBookmarkModel.getUnreadCount(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder()));
    }

    @Test
    public void testChildCount() {
        mBookmarkModel.addBookmark(
                mBookmarkModel.getOtherFolderId(), 0, "title1", new GURL("https://test1.com"));
        mBookmarkModel.addBookmark(
                mBookmarkModel.getOtherFolderId(), 0, "title2", new GURL("https://test2.com"));
        mBookmarkModel.addBookmark(
                mBookmarkModel.getOtherFolderId(), 0, "title3", new GURL("https://test3.com"));
        BookmarkId folder =
                mBookmarkModel.addFolder(mBookmarkModel.getOtherFolderId(), 0, "folder1");
        mBookmarkModel.addBookmark(folder, 0, "title11", new GURL("https://test11.com"));
        mBookmarkModel.addBookmark(folder, 0, "title12", new GURL("https://test12.com"));
        mBookmarkModel.addBookmark(folder, 0, "title13", new GURL("https://test13.com"));
        assertEquals(4, mBookmarkModel.getChildCount(mBookmarkModel.getOtherFolderId()));
        assertEquals(7, mBookmarkModel.getTotalBookmarkCount(mBookmarkModel.getOtherFolderId()));

        mBookmarkModel.addToReadingList(
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                "title1",
                new GURL("https://test1.com"));
        mBookmarkModel.addToReadingList(
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                "title2",
                new GURL("https://test2.com"));
        mBookmarkModel.addToReadingList(
                mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                "title3",
                new GURL("https://test3.com"));
        assertEquals(
                3,
                mBookmarkModel.getChildCount(mBookmarkModel.getLocalOrSyncableReadingListFolder()));
        assertEquals(
                3,
                mBookmarkModel.getTotalBookmarkCount(
                        mBookmarkModel.getLocalOrSyncableReadingListFolder()));
    }
}
