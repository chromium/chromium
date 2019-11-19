// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for {@link BookmarkModel}, the data layer of bookmarks.
 */
@RetryOnFailure(message = "crbug.com/740786")
@RunWith(BaseJUnit4ClassRunner.class)
public class BookmarkModelTest {
    @Rule
    public final RuleChain mChain =
            RuleChain.outerRule(new ChromeBrowserTestRule()).around(new UiThreadTestRule());

    private static final int TIMEOUT_MS = 5000;
    private BookmarkModel mBookmarkModel;
    private BookmarkId mMobileNode;
    private BookmarkId mOtherNode;
    private BookmarkId mDesktopNode;

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedProfile();
            mBookmarkModel = new BookmarkModel(profile);
            mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
        });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMobileNode = mBookmarkModel.getMobileFolderId();
            mDesktopNode = mBookmarkModel.getDesktopFolderId();
            mOtherNode = mBookmarkModel.getOtherFolderId();
        });
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testBookmarkPropertySetters() {
        BookmarkId folderA = mBookmarkModel.addFolder(mMobileNode, 0, "a");

        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(mMobileNode, 0, "a", "http://a.com");
        BookmarkId bookmarkC = addBookmark(mOtherNode, 0, "a", "http://a.com");
        BookmarkId bookmarkD = addBookmark(folderA, 0, "a", "http://a.com");

        mBookmarkModel.setBookmarkTitle(folderA, "hauri");
        Assert.assertEquals("hauri", mBookmarkModel.getBookmarkTitle(folderA));

        mBookmarkModel.setBookmarkTitle(bookmarkA, "auri");
        mBookmarkModel.setBookmarkUrl(bookmarkA, "http://auri.org/");
        verifyBookmark(bookmarkA, "auri", "http://auri.org/", false, mDesktopNode);

        mBookmarkModel.setBookmarkTitle(bookmarkB, "lauri");
        mBookmarkModel.setBookmarkUrl(bookmarkB, "http://lauri.org/");
        verifyBookmark(bookmarkB, "lauri", "http://lauri.org/", false, mMobileNode);

        mBookmarkModel.setBookmarkTitle(bookmarkC, "mauri");
        mBookmarkModel.setBookmarkUrl(bookmarkC, "http://mauri.org/");
        verifyBookmark(bookmarkC, "mauri", "http://mauri.org/", false, mOtherNode);

        mBookmarkModel.setBookmarkTitle(bookmarkD, "kauri");
        mBookmarkModel.setBookmarkUrl(bookmarkD, "http://kauri.org/");
        verifyBookmark(bookmarkD, "kauri", "http://kauri.org/", false, folderA);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testMoveBookmarks() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", "http://b.com");
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", "http://c.com");
        BookmarkId folderA = mBookmarkModel.addFolder(mOtherNode, 0, "fa");
        BookmarkId folderB = mBookmarkModel.addFolder(mDesktopNode, 0, "fb");
        BookmarkId folderC = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkAA = addBookmark(folderA, 0, "aa", "http://aa.com");
        BookmarkId folderAA = mBookmarkModel.addFolder(folderA, 0, "faa");

        HashSet<BookmarkId> movedBookmarks = new HashSet<BookmarkId>(6);
        movedBookmarks.add(bookmarkA);
        movedBookmarks.add(bookmarkB);
        movedBookmarks.add(bookmarkC);
        movedBookmarks.add(folderC);
        movedBookmarks.add(folderB);
        movedBookmarks.add(bookmarkAA);
        mBookmarkModel.moveBookmarks(new ArrayList<BookmarkId>(movedBookmarks), folderAA);

        // Order of the moved bookmarks is not tested.
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIDs(folderAA, true, true), movedBookmarks);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testMoveBookmarksToSameFolder() {
        BookmarkId folder = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        BookmarkId bookmarkA = addBookmark(folder, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(folder, 1, "b", "http://b.com");

        HashSet<BookmarkId> movedBookmarks = new HashSet<BookmarkId>(2);
        movedBookmarks.add(bookmarkA);
        movedBookmarks.add(bookmarkB);
        mBookmarkModel.moveBookmarks(new ArrayList<BookmarkId>(movedBookmarks), folder);

        // Order of the moved bookmarks is not tested.
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIDs(folder, true, true), movedBookmarks);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testDeleteBookmarks() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", "http://b.com");
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", "http://c.com");

        // Dete a single bookmark
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
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", "http://b.com");
        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", "http://c.com");

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
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        expectedChildren.add(addBookmark(folderA, 0, "a", "http://a.com"));
        BookmarkId folderAA = mBookmarkModel.addFolder(folderA, 0, "faa");
        // urls only
        verifyBookmarkListNoOrder(
                mBookmarkModel.getChildIDs(folderA, false, true), expectedChildren);
        // folders only
        verifyBookmarkListNoOrder(mBookmarkModel.getChildIDs(folderA, true, false),
                new HashSet<BookmarkId>(Arrays.asList(folderAA)));
        // folders and urls
        expectedChildren.add(folderAA);
        verifyBookmarkListNoOrder(
                mBookmarkModel.getChildIDs(folderA, true, true), expectedChildren);
    }

    // Moved from BookmarkBridgeTest
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"Bookmark"})
    public void testAddBookmarksAndFolders() {
        BookmarkId bookmarkA = addBookmark(mDesktopNode, 0, "a", "http://a.com");
        verifyBookmark(bookmarkA, "a", "http://a.com/", false, mDesktopNode);

        BookmarkId bookmarkB = addBookmark(mOtherNode, 0, "b", "http://b.com");
        verifyBookmark(bookmarkB, "b", "http://b.com/", false, mOtherNode);

        BookmarkId bookmarkC = addBookmark(mMobileNode, 0, "c", "http://c.com");
        verifyBookmark(bookmarkC, "c", "http://c.com/", false, mMobileNode);

        BookmarkId folderA = mBookmarkModel.addFolder(mOtherNode, 0, "fa");
        verifyBookmark(folderA, "fa", null, true, mOtherNode);

        BookmarkId folderB = mBookmarkModel.addFolder(mDesktopNode, 0, "fb");
        verifyBookmark(folderB, "fb", null, true, mDesktopNode);

        BookmarkId folderC = mBookmarkModel.addFolder(mMobileNode, 0, "fc");
        verifyBookmark(folderC, "fc", null, true, mMobileNode);

        BookmarkId bookmarkAA = addBookmark(folderA, 0, "aa", "http://aa.com");
        verifyBookmark(bookmarkAA, "aa", "http://aa.com/", false, folderA);

        BookmarkId folderAA = mBookmarkModel.addFolder(folderA, 0, "faa");
        verifyBookmark(folderAA, "faa", null, true, folderA);
    }

    private BookmarkId addBookmark(final BookmarkId parent, final int index, final String title,
            final String url) {
        final AtomicReference<BookmarkId> result = new AtomicReference<BookmarkId>();
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            result.set(mBookmarkModel.addBookmark(parent, index, title, url));
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

    private void verifyBookmark(BookmarkId idToVerify, String expectedTitle,
            String expectedUrl, boolean isFolder, BookmarkId expectedParent) {
        Assert.assertNotNull(idToVerify);
        BookmarkItem item = mBookmarkModel.getBookmarkById(idToVerify);
        Assert.assertEquals(expectedTitle, item.getTitle());
        Assert.assertEquals(isFolder, item.isFolder());
        if (!isFolder) Assert.assertEquals(expectedUrl, item.getUrl());
        Assert.assertEquals(expectedParent, item.getParentId());
    }

    /**
     * Before using this helper method, always make sure @param listToVerify does not contain
     * duplicates.
     */
    private void verifyBookmarkListNoOrder(List<BookmarkId> listToVerify,
            HashSet<BookmarkId> expectedIds) {
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
