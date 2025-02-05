// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.endsWith;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/** Integration tests for the bookmark bar feature. */
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR)
@Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@RunWith(ChromeJUnit4ClassRunner.class)
public class BookmarkBarTest {

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    private BookmarkModel mModel;
    private BookmarkId mBookmarkId;
    private BookmarkId mDesktopFolderId;

    @Before
    public void setUp() {
        mInitialStateRule.startOnBlankPage();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = sActivityTestRule.getActivity().getBookmarkModelForTesting();
                    mModel.removeAllUserBookmarks();
                    mDesktopFolderId = mModel.getDesktopFolderId();
                });
    }

    @After
    public void tearDown() {
        if (mBookmarkId != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mModel.deleteBookmark(mBookmarkId));
            mBookmarkId = null;
        }
    }

    private @Nullable BookmarkId addBookmark(int index, @NonNull String title, @NonNull GURL url)
            throws ExecutionException {
        return BookmarkTestUtil.addBookmark(
                sActivityTestRule, mModel, index, title, url, /* parent= */ mDesktopFolderId);
    }

    private @Nullable BookmarkId addFolder(@NonNull String title) throws ExecutionException {
        return BookmarkTestUtil.addFolder(
                sActivityTestRule, mModel, title, /* parent= */ mDesktopFolderId);
    }

    private @NonNull Matcher<View> bookmarkBarItemWithText(@NonNull String text) {
        return allOf(isDescendantOfA(withClassName(endsWith("BookmarkBar"))), withText(text));
    }

    private @NonNull Matcher<View> bookmarkManagerToolbarWithText(@NonNull String text) {
        return allOf(isDescendantOfA(withClassName(endsWith("BookmarkToolbar"))), withText(text));
    }

    private @NonNull GURL getTestServerUrl(@NonNull String relativeUrl) {
        return new GURL(sActivityTestRule.getTestServer().getURL(relativeUrl));
    }

    @Test
    @MediumTest
    public void testOnAllBookmarksButtonClick() {
        onViewWaiting(bookmarkBarItemWithText("All Bookmarks")).perform(click());
        onViewWaiting(bookmarkManagerToolbarWithText("Bookmarks")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOnBookmarkFolderClick() throws ExecutionException {
        final String title = "Folder";
        mBookmarkId = addFolder(title);
        onViewWaiting(bookmarkBarItemWithText(title)).perform(click());
        onViewWaiting(bookmarkManagerToolbarWithText(title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testOnBookmarkItemClick() throws ExecutionException {
        final String title = "Google";
        final GURL url = getTestServerUrl("/chrome/test/data/android/google.html");
        mBookmarkId = addBookmark(/* index= */ 0, title, url);
        onViewWaiting(bookmarkBarItemWithText(title)).perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    final Tab activityTab = sActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(activityTab, notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), is(url));
                });
    }
}
