// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/** Tests for bookmark bridge */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BookmarkBridgeTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private BookmarkBridge mBookmarkBridge;
    private BookmarkId mMobileNode;
    private BookmarkId mOtherNode;
    private BookmarkId mDesktopNode;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mBookmarkBridge = BookmarkBridge.getForProfile(profile);
                    mBookmarkBridge.loadFakePartnerBookmarkShimForTesting();
                });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMobileNode = mBookmarkBridge.getMobileFolderId();
                    mDesktopNode = mBookmarkBridge.getDesktopFolderId();
                    mOtherNode = mBookmarkBridge.getOtherFolderId();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkBridge.removeAllUserBookmarks());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testAddBookmarksAndFolders() {
        BookmarkId bookmarkA =
                mBookmarkBridge.addBookmark(mDesktopNode, 0, "a", new GURL("http://a.com"));
        verifyBookmark(bookmarkA, "a", "http://a.com/", false, mDesktopNode);
        BookmarkId bookmarkB =
                mBookmarkBridge.addBookmark(mOtherNode, 0, "b", new GURL("http://b.com"));
        verifyBookmark(bookmarkB, "b", "http://b.com/", false, mOtherNode);
        BookmarkId bookmarkC =
                mBookmarkBridge.addBookmark(mMobileNode, 0, "c", new GURL("http://c.com"));
        verifyBookmark(bookmarkC, "c", "http://c.com/", false, mMobileNode);
        BookmarkId folderA = mBookmarkBridge.addFolder(mOtherNode, 0, "fa");
        verifyBookmark(folderA, "fa", null, true, mOtherNode);
        BookmarkId folderB = mBookmarkBridge.addFolder(mDesktopNode, 0, "fb");
        verifyBookmark(folderB, "fb", null, true, mDesktopNode);
        BookmarkId folderC = mBookmarkBridge.addFolder(mMobileNode, 0, "fc");
        verifyBookmark(folderC, "fc", null, true, mMobileNode);
        BookmarkId bookmarkAA =
                mBookmarkBridge.addBookmark(folderA, 0, "aa", new GURL("http://aa.com"));
        verifyBookmark(bookmarkAA, "aa", "http://aa.com/", false, folderA);
        BookmarkId folderAA = mBookmarkBridge.addFolder(folderA, 0, "faa");
        verifyBookmark(folderAA, "faa", null, true, folderA);
    }

    private void verifyBookmark(
            BookmarkId idToVerify,
            String expectedTitle,
            String expectedUrl,
            boolean isFolder,
            BookmarkId expectedParent) {
        Assert.assertNotNull(idToVerify);
        BookmarkItem item = mBookmarkBridge.getBookmarkById(idToVerify);
        Assert.assertEquals(expectedTitle, item.getTitle());
        Assert.assertEquals(item.isFolder(), isFolder);
        if (!isFolder) Assert.assertEquals(expectedUrl, item.getUrl().getSpec());
        Assert.assertEquals(item.getParentId(), expectedParent);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testGetAllFoldersWithDepths() {
        BookmarkId folderA = mBookmarkBridge.addFolder(mMobileNode, 0, "a");
        BookmarkId folderB = mBookmarkBridge.addFolder(mDesktopNode, 0, "b");
        BookmarkId folderC = mBookmarkBridge.addFolder(mOtherNode, 0, "c");
        BookmarkId folderAA = mBookmarkBridge.addFolder(folderA, 0, "aa");
        BookmarkId folderBA = mBookmarkBridge.addFolder(folderB, 0, "ba");
        BookmarkId folderAAA = mBookmarkBridge.addFolder(folderAA, 0, "aaa");
        BookmarkId folderAAAA = mBookmarkBridge.addFolder(folderAAA, 0, "aaaa");

        mBookmarkBridge.addBookmark(mMobileNode, 0, "ua", new GURL("http://www.google.com"));
        mBookmarkBridge.addBookmark(mDesktopNode, 0, "ua", new GURL("http://www.google.com"));
        mBookmarkBridge.addBookmark(mOtherNode, 0, "ua", new GURL("http://www.google.com"));
        mBookmarkBridge.addBookmark(folderA, 0, "ua", new GURL("http://www.medium.com"));

        // Map folders to depths as expected results
        HashMap<BookmarkId, Integer> idToDepth = new HashMap<>();
        idToDepth.put(mMobileNode, 0);
        idToDepth.put(folderA, 1);
        idToDepth.put(folderAA, 2);
        idToDepth.put(folderAAA, 3);
        idToDepth.put(folderAAAA, 4);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(folderB, 1);
        idToDepth.put(folderBA, 2);
        idToDepth.put(mOtherNode, 0);
        idToDepth.put(folderC, 1);

        List<BookmarkId> folderList = new ArrayList<>();
        List<Integer> depthList = new ArrayList<>();
        mBookmarkBridge.getAllFoldersWithDepths(folderList, depthList);
        verifyFolderDepths(folderList, depthList, idToDepth);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testGetMoveDestinations() {
        BookmarkId folderA = mBookmarkBridge.addFolder(mMobileNode, 0, "a");
        BookmarkId folderB = mBookmarkBridge.addFolder(mDesktopNode, 0, "b");
        BookmarkId folderC = mBookmarkBridge.addFolder(mOtherNode, 0, "c");
        BookmarkId folderAA = mBookmarkBridge.addFolder(folderA, 0, "aa");
        BookmarkId folderBA = mBookmarkBridge.addFolder(folderB, 0, "ba");
        BookmarkId folderAAA = mBookmarkBridge.addFolder(folderAA, 0, "aaa");

        mBookmarkBridge.addBookmark(mMobileNode, 0, "ua", new GURL("http://www.google.com"));
        mBookmarkBridge.addBookmark(mDesktopNode, 0, "ua", new GURL("http://www.google.com"));
        mBookmarkBridge.addBookmark(mOtherNode, 0, "ua", new GURL("http://www.google.com"));
        mBookmarkBridge.addBookmark(folderA, 0, "ua", new GURL("http://www.medium.com"));

        // Map folders to depths as expected results
        HashMap<BookmarkId, Integer> idToDepth = new HashMap<>();

        List<BookmarkId> folderList = new ArrayList<>();
        List<Integer> depthList = new ArrayList<>();

        mBookmarkBridge.getMoveDestinations(folderList, depthList, Arrays.asList(folderA));
        idToDepth.put(mMobileNode, 0);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(folderB, 1);
        idToDepth.put(folderBA, 2);
        idToDepth.put(mOtherNode, 0);
        idToDepth.put(folderC, 1);
        verifyFolderDepths(folderList, depthList, idToDepth);

        mBookmarkBridge.getMoveDestinations(folderList, depthList, Arrays.asList(folderB));
        idToDepth.put(mMobileNode, 0);
        idToDepth.put(folderA, 1);
        idToDepth.put(folderAA, 2);
        idToDepth.put(folderAAA, 3);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(mOtherNode, 0);
        idToDepth.put(folderC, 1);
        verifyFolderDepths(folderList, depthList, idToDepth);

        mBookmarkBridge.getMoveDestinations(folderList, depthList, Arrays.asList(folderC));
        idToDepth.put(mMobileNode, 0);
        idToDepth.put(folderA, 1);
        idToDepth.put(folderAA, 2);
        idToDepth.put(folderAAA, 3);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(folderB, 1);
        idToDepth.put(folderBA, 2);
        idToDepth.put(mOtherNode, 0);
        verifyFolderDepths(folderList, depthList, idToDepth);

        mBookmarkBridge.getMoveDestinations(folderList, depthList, Arrays.asList(folderBA));
        idToDepth.put(mMobileNode, 0);
        idToDepth.put(folderA, 1);
        idToDepth.put(folderAA, 2);
        idToDepth.put(folderAAA, 3);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(folderB, 1);
        idToDepth.put(mOtherNode, 0);
        idToDepth.put(folderC, 1);
        verifyFolderDepths(folderList, depthList, idToDepth);

        mBookmarkBridge.getMoveDestinations(
                folderList, depthList, Arrays.asList(folderAA, folderC));
        idToDepth.put(mMobileNode, 0);
        idToDepth.put(folderA, 1);
        idToDepth.put(mDesktopNode, 0);
        idToDepth.put(folderB, 1);
        idToDepth.put(folderBA, 2);
        idToDepth.put(mOtherNode, 0);
        verifyFolderDepths(folderList, depthList, idToDepth);
    }

    private void verifyFolderDepths(
            List<BookmarkId> folderList,
            List<Integer> depthList,
            HashMap<BookmarkId, Integer> idToDepth) {
        Assert.assertEquals(folderList.size(), depthList.size());
        Assert.assertEquals(folderList.size(), idToDepth.size());
        for (int i = 0; i < folderList.size(); i++) {
            BookmarkId folder = folderList.get(i);
            Integer depth = depthList.get(i);
            Assert.assertNotNull(folder);
            Assert.assertNotNull(depthList.get(i));
            Assert.assertTrue(
                    "Folder list contains non-folder elements: ",
                    mBookmarkBridge.getBookmarkById(folder).isFolder());
            Assert.assertTrue(
                    "Returned list contained unexpected key: ", idToDepth.containsKey(folder));
            Assert.assertEquals(idToDepth.get(folder), depth);
            idToDepth.remove(folder);
        }
        Assert.assertEquals(idToDepth.size(), 0);
        folderList.clear();
        depthList.clear();
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testReorderBookmarks() {
        long kAFolder = mBookmarkBridge.addFolder(mMobileNode, 0, "a").getId();
        long kBFolder = mBookmarkBridge.addFolder(mMobileNode, 0, "b").getId();
        long kAUrl =
                mBookmarkBridge.addBookmark(mMobileNode, 0, "a", new GURL("http://a.com")).getId();
        long kBUrl =
                mBookmarkBridge.addBookmark(mMobileNode, 0, "b", new GURL("http://b.com")).getId();
        // Magic folder for partner bookmarks. See fake loading of partner bookmarks in setUp.
        long kPartnerBookmarks = 0;

        long[] startingIdsArray = new long[] {kBUrl, kAUrl, kBFolder, kAFolder, kPartnerBookmarks};
        Assert.assertArrayEquals(
                startingIdsArray, getIdArray(mBookmarkBridge.getChildIds(mMobileNode)));

        long[] reorderedIdsArray = new long[] {kAUrl, kBFolder, kBUrl, kAFolder};
        mBookmarkBridge.reorderBookmarks(mMobileNode, reorderedIdsArray);

        long[] endingIdsArray = new long[] {kAUrl, kBFolder, kBUrl, kAFolder, kPartnerBookmarks};
        Assert.assertArrayEquals(
                endingIdsArray, getIdArray(mBookmarkBridge.getChildIds(mMobileNode)));
    }

    /**
     * Given a list of BookmarkIds, returns an array full of the (long) ID numbers.
     *
     * @param bIds The BookmarkIds of interest.
     * @return An array containing the (long) ID numbers.
     */
    private long[] getIdArray(List<BookmarkId> bIds) {
        // Get the new order for the IDs.
        long[] newOrder = new long[bIds.size()];
        for (int i = 0; i <= bIds.size() - 1; i++) {
            newOrder[i] = bIds.get(i).getId();
        }
        return newOrder;
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testSearchPartner() {
        List<BookmarkId> expectedSearchResults = new ArrayList<>();
        expectedSearchResults.add(
                new BookmarkId(
                        1, 1)); // Partner bookmark with ID 1: "Partner Bookmark A", http://a.com
        expectedSearchResults.add(
                new BookmarkId(
                        2, 1)); // Partner bookmark with ID 2: "Partner Bookmark B", http://b.com
        List<BookmarkId> searchResults = mBookmarkBridge.searchBookmarks("pArTnER BookMARK", 100);
        Assert.assertEquals(
                "Expected search results would yield partner bookmark with "
                        + "case-insensitive title match",
                expectedSearchResults,
                searchResults);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testSearchFolder() {
        List<BookmarkId> expectedSearchResults = new ArrayList<>();
        expectedSearchResults.add(mBookmarkBridge.addFolder(mMobileNode, 0, "FooBar"));
        List<BookmarkId> searchResults = mBookmarkBridge.searchBookmarks("oba", 100);
        Assert.assertEquals(
                "Expected search results would yield case-insensitive match of " + "part of title",
                expectedSearchResults,
                searchResults);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testSearch_MaxResults() {
        List<BookmarkId> expectedSearchResults = new ArrayList<>();
        expectedSearchResults.add(mBookmarkBridge.addFolder(mMobileNode, 0, "FooBar"));
        expectedSearchResults.add(mBookmarkBridge.addFolder(mMobileNode, 1, "BazQuux"));
        expectedSearchResults.add(
                new BookmarkId(
                        1, 1)); // Partner bookmark with ID 1: "Partner Bookmark A", http://a.com

        List<BookmarkId> searchResults = mBookmarkBridge.searchBookmarks("a", 3);
        Assert.assertEquals(
                "Expected search results size to be 3 (maximum size)", 3, searchResults.size());
        Assert.assertEquals(
                "Expected that user (non-partner) bookmarks would get priority "
                        + "over partner bookmarks",
                expectedSearchResults,
                searchResults);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @RequiresRestart
    public void testAddToReadingList() {
        Assert.assertNull(
                "Should return null for non http/https URLs.",
                mBookmarkBridge.addToDefaultReadingList("a", new GURL("chrome://flags")));
        BookmarkId readingListId =
                mBookmarkBridge.addToDefaultReadingList("a", new GURL("https://www.google.com/"));
        Assert.assertNotNull("Failed to add to reading list", readingListId);
        Assert.assertEquals(BookmarkType.READING_LIST, readingListId.getType());
        BookmarkItem readingListItem = mBookmarkBridge.getBookmarkById(readingListId);
        Assert.assertNotNull("Failed to find the reading list", readingListItem);
        Assert.assertEquals(
                "https://www.google.com/", readingListItem.getUrl().getValidSpecOrEmpty());
        Assert.assertEquals("a", readingListItem.getTitle());
    }
}
