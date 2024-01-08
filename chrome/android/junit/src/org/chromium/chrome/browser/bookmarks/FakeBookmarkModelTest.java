// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link FakeBookmarkModel}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FakeBookmarkModelTest {

    @Rule public Features.JUnitProcessor mFeaturesProcessorRule = new Features.JUnitProcessor();

    private FakeBookmarkModel mBookmarkModel;

    @Before
    public void setup() {
        mBookmarkModel = (FakeBookmarkModel) FakeBookmarkModel.createModel();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
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
    @EnableFeatures(ChromeFeatureList.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testDefaultFolders_accountStorageEnabled() {
        List<BookmarkId> expected =
                Arrays.asList(
                        mBookmarkModel.getOtherFolderId(),
                        mBookmarkModel.getDesktopFolderId(),
                        mBookmarkModel.getMobileFolderId(),
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        mBookmarkModel.getAccountReadingListFolder());
        assertEquals(expected, mBookmarkModel.getTopLevelFolderIds());

        expected = Arrays.asList(mBookmarkModel.getPartnerFolderId());
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getMobileFolderId()));
    }

    @Test
    public void testAddFolder() {
        BookmarkId id =
                mBookmarkModel.addFolder(mBookmarkModel.getOtherFolderId(), 0, "user folder");

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getOtherFolderId()));
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
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testAddAccountReadingListBokmark() {
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
}
