// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.matches;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/40743432): Once SyncTestRule supports batching, investigate batching this suite.
@DoNotBatch(reason = "SyncTestRule doesn't support batching.")
public class AccountBookmarkTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();
    @Rule public BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() throws Exception {
        // Auto form factors are very small, disable the promo to leave room for bookmarks.
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);
        mBookmarkModel =
                runOnUiThreadBlocking(
                        () ->
                                BookmarkModel.getForProfile(
                                        ProfileManager.getLastUsedRegularProfile()));
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mBookmarkManagerCoordinator =
                mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
    }

    @Test
    @SmallTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testAccountFoldersDisplay() {
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getAccountMobileFolderId() != null);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(
                mBookmarkManagerCoordinator.getRecyclerViewForTesting());
        checkTopLevelAccountFoldersDisplayed();
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    @DisabledTest(
            message =
                    "Enable this test when reading list is available w/o restart crbug.com/1510547")
    public void testOpenFromReadingListAndNavigateBack() throws Exception {
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getAccountReadingListFolder() != null);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(
                mBookmarkManagerCoordinator.getRecyclerViewForTesting());

        runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addToReadingList(
                                mBookmarkModel.getAccountReadingListFolder(),
                                "test",
                                new GURL("https://test.com")));

        BookmarkTestUtil.getRecyclerRowViewInteraction(
                        "Reading list", /* isAccountBookmark= */ true)
                .perform(click());
        onView(withText("test")).perform(click());
        Espresso.pressBack();
        onView(withText("test")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testDefaultFolders() {
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getAccountMobileFolderId() != null);
        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            mBookmarkModel.getAccountMobileFolderId(),
                            mBookmarkModel.getDefaultBookmarkFolder());
                });
    }

    private void checkTopLevelAccountFoldersDisplayed() {
        // TODO(crbug.com/41483140): This is currently broken because the account reading list
        // folder doesn't show up without a restart. This should be updated once that folder is
        // available.
        checkToolbarTitleMatches("Bookmarks");
        BookmarkTestUtil.getRecyclerRowViewInteraction(
                        "Mobile bookmarks", /* isAccountBookmark= */ true)
                .check(matches(isDisplayed()));
        BookmarkTestUtil.getRecyclerRowViewInteraction(
                        "Reading list", /* isAccountBookmark= */ true)
                .check(matches(isDisplayed()));
    }

    // Checks the toolbar title against the given string.
    private void checkToolbarTitleMatches(String text) {
        onView(allOf(isDescendantOfA(withId(R.id.action_bar)), withText(text)))
                .check(matches(isDisplayed()));
    }
}
