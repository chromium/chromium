// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests functionality in {@link BookmarkFolderPickerActivity}. */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
public class BookmarkFolderPickerActivityTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule public static TestRule sFeaturesProcessorRule = new Features.JUnitProcessor();

    private static BookmarkModel sBookmarkModel;
    private static BookmarkId sMobileFolderId;
    private static BookmarkId sOtherFolderId;

    private BookmarkFolderPickerActivity mActivity;

    @BeforeClass
    public static void setUpBeforeClass() throws TimeoutException {
        sActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel =
                            BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
                    sBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sMobileFolderId = sBookmarkModel.getMobileFolderId();
                    sOtherFolderId = sBookmarkModel.getOtherFolderId();
                });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sBookmarkModel.removeAllUserBookmarks();
                });
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testMoveBookmark() throws ExecutionException, TimeoutException, Exception {
        BookmarkId bookmark =
                addBookmark(sMobileFolderId, 0, "bookmark", new GURL("https://google.com"));
        BookmarkId folder = addFolder(sMobileFolderId, 1, "folder");
        startFolderPickerActivity(bookmark);

        onView(withText("folder")).perform(click());
        onView(withText("Move here")).perform(click());

        BookmarkItem item = getBookmarkItem(bookmark);
        assertEquals(folder, item.getParentId());

        CriteriaHelper.pollUiThread(() -> mActivity.isFinishing());
    }

    @Test
    @MediumTest
    @Feature({"Bookmark"})
    public void testCancelButton()
            throws ExecutionException, TimeoutException, InterruptedException {
        BookmarkId bookmark =
                addBookmark(sMobileFolderId, 0, "bookmark", new GURL("https://google.com"));
        startFolderPickerActivity(bookmark);

        onView(withText("Cancel")).perform(click());

        CriteriaHelper.pollUiThread(() -> mActivity.isFinishing());
    }

    private BookmarkItem getBookmarkItem(BookmarkId bookmarkId) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.getBookmarkById(bookmarkId));
    }

    private BookmarkId addFolder(BookmarkId parent, int index, String title)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.addFolder(parent, index, title));
    }

    private BookmarkId addBookmark(BookmarkId parent, int index, String title, GURL url)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> sBookmarkModel.addBookmark(parent, index, title, url));
    }

    private void startFolderPickerActivity(BookmarkId... ids) {
        // TODO(crbug.com/): Move this code to a shared ActivityTestUtils location.
        BookmarkUtils.startFolderPickerActivity(sActivityTestRule.getActivity(), ids);
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getLastTrackedFocusedActivity()
                                instanceof BookmarkFolderPickerActivity,
                "Timed out waiting for BookmarkFolderPickerActivity");
        mActivity =
                (BookmarkFolderPickerActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        CriteriaHelper.pollUiThread(
                () -> ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED,
                "Timed out waiting for activity to enter the RESUMED state.");
    }
}
