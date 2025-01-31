// Copyright 2015 The Chromium Authors
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link BookmarkModel}, the data layer of bookmarks. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class BookmarkModelTest {
    public static final GURL A_COM = new GURL("http://a.com");
    public static final GURL B_COM = new GURL("http://b.com");
    public static final GURL C_COM = new GURL("http://c.com");
    public static final GURL AA_COM = new GURL("http://aa.com");

    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private static final int TIMEOUT_MS = 5000;
    private BookmarkModel mBookmarkModel;
    private BookmarkId mMobileNode;
    private BookmarkId mOtherNode;
    private BookmarkId mDesktopNode;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mBookmarkModel = BookmarkModel.getForProfile(profile);
                    mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMobileNode = mBookmarkModel.getMobileFolderId();
                    mDesktopNode = mBookmarkModel.getDesktopFolderId();
                    mOtherNode = mBookmarkModel.getOtherFolderId();
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBookmarkModel.removeAllUserBookmarks());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testBookmarkPropertySetters() {
        BookmarkId folderA = mBookmarkModel.addFolder(mMobileNode, 0, "a");

        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", A_COM);
        BookmarkId bookmarkB = addBookmark(mMobileNode, 0, "a", A_COM);
        BookmarkId bookmarkC = addBookmark(mOtherNode, 0, "a", A_COM);
        BookmarkId bookmarkD = addBookmark(folderA, 0, "a", A_COM);

        mBookmarkModel.setBookmarkTitle(folderA, "hauri");
        Assert.assertEquals("hauri", mBookmarkModel.getBookmarkTitle(folderA));

        mBookmarkModel.setBookmarkTitle(bookmarkA, "auri");
        mBookmarkModel.setBookmarkUrl(bookmarkA, new GURL("http://auri.org/"));
        verifyBookmark(bookmarkA, "auri", "http://auri.org/", false, mDesktopNode);

        mBookmarkModel.setBookmarkTitle(bookmarkB, "lauri");
        mBookmarkModel.setBookmarkUrl(bookmarkB, new GURL("http://lauri.org/"));
        verifyBookmark(bookmarkB, "lauri", "http://lauri.org/", false, mMobileNode);

        mBookmarkModel.setBookmarkTitle(bookmarkC, "mauri");
        mBookmarkModel.setBookmarkUrl(bookmarkC, new GURL("http://mauri.org/"));
        verifyBookmark(bookmarkC, "mauri", "http://mauri.org/", false, mOtherNode);

        mBookmarkModel.setBookmarkTitle(bookmarkD, "kauri");
        mBookmarkModel.setBookmarkUrl(bookmarkD, new GURL("http://kauri.org/"));
        verifyBookmark(bookmarkD, "kauri", "http://kauri.org/", false, folderA);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testMoveBookmarks() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", A_COM);
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", B_COM);
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", C_COM);
        BookmarkId folderA = mBookmarkModel.addFolder(mOtherNode, 0, "fa");
        BookmarkId folderB = mBookmarkModel.addFolder(mDesktopNode, 0, "fb");
        BookmarkId folderC = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkAA = addBookmark(folderA, 0, "aa", AA_COM);
        BookmarkId folderAA = mBookmarkModel.addFolder(folderA, 0, "faa");

        HashSet<BookmarkId> movedBookmarks = new HashSet<>(6);
        movedBookmarks.add(bookmarkA);
        movedBookmarks.add(bookmarkB);
        movedBookmarks.add(bookmarkC);
        movedBookmarks.add(folderC);
        movedBookmarks.add(folderB);
        movedBookmarks.add(bookmarkAA);
        mBookmarkModel.moveBookmarks(new ArrayList<>(movedBookmarks), folderAA);

        // Order of the moved bookmarks is not tested.
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIds(folderAA), movedBookmarks);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testMoveBookmarksToSameFolder() {
        BookmarkId folder = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkA = addBookmark(folder, 0, "a", A_COM);
        BookmarkId bookmarkB = addBookmark(folder, 1, "b", B_COM);

        HashSet<BookmarkId> movedBookmarks = new HashSet<>(2);
        movedBookmarks.add(bookmarkA);
        movedBookmarks.add(bookmarkB);
        mBookmarkModel.moveBookmarks(new ArrayList<>(movedBookmarks), folder);

        // Order of the moved bookmarks is not tested.
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIds(folder), movedBookmarks);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testMoveBookmarksMixed() {
        // Inspired by https://crbug.com/1441847 where a move during a search would have bookmarks
        // from a mixed set of parent folders. Need to be able to handle interleaving url bookmarks
        // where only some of which are in the same destination folder.
        BookmarkId folderA = mBookmarkModel.addFolder(mMobileNode, 0, "fa");
        BookmarkId folderC = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkA = addBookmark(folderA, 0, "a", A_COM);
        BookmarkId bookmarkB = addBookmark(folderA, 1, "b", B_COM);
        BookmarkId bookmarkC = addBookmark(folderC, 0, "c", C_COM);

        List<BookmarkId> movedBookmarks = new ArrayList<>();
        movedBookmarks.add(bookmarkA);
        movedBookmarks.add(bookmarkC);
        movedBookmarks.add(bookmarkB);
        mBookmarkModel.moveBookmarks(movedBookmarks, folderC);

        verifyBookmarkListNoOrder(mBookmarkModel.getChildIds(folderA), Collections.emptyList());
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIds(folderC), movedBookmarks);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testDeleteBookmarks() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", A_COM);
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", B_COM);
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", C_COM);

        // Delete a single bookmark.
        mBookmarkModel.deleteBookmarks(bookmarkA);
        Assert.assertNull(mBookmarkModel.getBookmarkById(bookmarkA));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkB));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkC));

        mBookmarkModel.undo();
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkA));

        // Delete and undo deletion of multiple bookmarks.
        mBookmarkModel.deleteBookmarks(bookmarkA, bookmarkB);

        Assert.assertNull(mBookmarkModel.getBookmarkById(bookmarkA));
        Assert.assertNull(mBookmarkModel.getBookmarkById(bookmarkB));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkC));

        mBookmarkModel.undo();

        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkA));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkB));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkC));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testDeleteBookmarksRepeatedly() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", A_COM);
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", B_COM);
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", C_COM);

        mBookmarkModel.deleteBookmarks(bookmarkA);

        // This line is problematic, see: https://crbug.com/824559
        mBookmarkModel.deleteBookmarks(bookmarkA, bookmarkB);

        Assert.assertNull(mBookmarkModel.getBookmarkById(bookmarkA));
        Assert.assertNull(mBookmarkModel.getBookmarkById(bookmarkB));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkC));

        // Only bookmark B should be undeleted here.
        mBookmarkModel.undo();

        Assert.assertNull(mBookmarkModel.getBookmarkById(bookmarkA));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkB));
        Assert.assertNotNull(mBookmarkModel.getBookmarkById(bookmarkC));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testGetChildIDs() {
        BookmarkId folderA = mBookmarkModel.addFolder(mMobileNode, 0, "fa");
        HashSet<BookmarkId> expectedChildren = new HashSet<>();
        expectedChildren.add(addBookmark(folderA, 0, "a", A_COM));
        expectedChildren.add(addBookmark(folderA, 0, "a", A_COM));
        expectedChildren.add(addBookmark(folderA, 0, "a", A_COM));
        expectedChildren.add(addBookmark(folderA, 0, "a", A_COM));
        BookmarkId folderAA = mBookmarkModel.addFolder(folderA, 0, "faa");
        // folders and urls
        expectedChildren.add(folderAA);
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIds(folderA), expectedChildren);
    }

    // Moved from BookmarkBridgeTest
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testAddBookmarksAndFolders() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", A_COM);
        verifyBookmark(bookmarkA, "a", "http://a.com/", false, mDesktopNode);

        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", B_COM);
        verifyBookmark(bookmarkB, "b", "http://b.com/", false, mOtherNode);

        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", C_COM);
        verifyBookmark(bookmarkC, "c", "http://c.com/", false, mMobileNode);

        BookmarkId folderA = mBookmarkModel.addFolder(mOtherNode, 0, "fa");
        verifyBookmark(folderA, "fa", null, true, mOtherNode);

        BookmarkId folderB = mBookmarkModel.addFolder(mDesktopNode, 0, "fb");
        verifyBookmark(folderB, "fb", null, true, mDesktopNode);

        BookmarkId folderC = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        verifyBookmark(folderC, "fc", null, true, mMobileNode);

        BookmarkId bookmarkAA = addBookmark(folderA, 0, "aa", AA_COM);
        verifyBookmark(bookmarkAA, "aa", "http://aa.com/", false, folderA);

        BookmarkId folderAA = mBookmarkModel.addFolder(folderA, 0, "faa");
        verifyBookmark(folderAA, "faa", null, true, folderA);
    }

    private BookmarkId addBookmark(
            final BookmarkId parent, final int index, final String title, final GURL url) {
        return addBookmark(mBookmarkModel, parent, index, title, url);
    }

    public static BookmarkId addBookmark(
            BookmarkModel model,
            final BookmarkId parent,
            final int index,
            final String title,
            final GURL url) {
        final AtomicReference<BookmarkId> result = new AtomicReference<>();
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    result.set(model.addBookmark(parent, index, title, url));
                    semaphore.release();
                });
        try {
            if (semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                return result.get();
            } else {
                return null;
            }
        } catch (InterruptedException e) {
            return null;
        }
    }

    private void verifyBookmark(
            BookmarkId idToVerify,
            String expectedTitle,
            String expectedUrl,
            boolean isFolder,
            BookmarkId expectedParent) {
        Assert.assertNotNull(idToVerify);
        BookmarkItem item = mBookmarkModel.getBookmarkById(idToVerify);
        Assert.assertEquals(expectedTitle, item.getTitle());
        Assert.assertEquals(isFolder, item.isFolder());
        if (!isFolder) Assert.assertEquals(expectedUrl, item.getUrl().getSpec());
        Assert.assertEquals(expectedParent, item.getParentId());
    }

    /**
     * Before using this helper method, always make sure @param listToVerify does not contain
     * duplicates.
     */
    private void verifyBookmarkListNoOrder(
            List<BookmarkId> listToVerify, Collection<BookmarkId> expectedIds) {
        HashSet<BookmarkId> expectedIdsCopy = new HashSet<>(expectedIds);
        Assert.assertEquals(expectedIdsCopy.size(), listToVerify.size());
        for (BookmarkId id : listToVerify) {
            Assert.assertNotNull(id);
            Assert.assertTrue("List contains wrong element: ", expectedIdsCopy.contains(id));
            expectedIdsCopy.remove(id);
        }
        Assert.assertTrue(
                "List does not contain some expected bookmarks: ", expectedIdsCopy.isEmpty());
    }
}
