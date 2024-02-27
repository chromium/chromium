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

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE})
// TODO(crbug.com/1168590): Once SyncTestRule supports batching, investigate batching this suite.
@DoNotBatch(reason = "SyncTestRule doesn't support batching.")
public class AccountBookmarkTest {
    private static final String BOOKMARKS_TYPE_STRING = "Bookmarks";

    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();
    @Rule public BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private BookmarkModel mBookmarkModel;
    private RecyclerView mRecyclerView;

    @Before
    public void setUp() throws Exception {
        // Auto form factors are very small, disable the promo to leave room for bookmarks.
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testReplaceSyncPromosWithSigninPromos() {
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getAccountMobileFolderId() != null);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(
                mBookmarkManagerCoordinator.getRecyclerViewForTesting());
        checkTopLevelAccountFoldersDisplayed();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testEnableDatatypesManually() {
        mSyncTestRule.setSelectedTypes(true, null);
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getAccountMobileFolderId() != null);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(
                mBookmarkManagerCoordinator.getRecyclerViewForTesting());
        checkTopLevelAccountFoldersDisplayed();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @DisabledTest(
            message =
                    "Enable this test when reading list is available w/o restart crbug.com/1510547")
    public void testOpenFromReadingListAndNavigateBack() throws Exception {
        mSyncTestRule.setSelectedTypes(true, null);
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        // TODO(crbug.com/1510547): This is currently broken because the account reading list
        // folder doesn't show up without a restart. This should be updated once that folder is
        // available.
        checkToolbarTitleMatches("Bookmarks");
        onView(withText("In your Google Account")).check(matches(isDisplayed()));
        BookmarkTestUtil.getRecyclerRowViewInteraction("Mobile bookmarks", true)
                .check(matches(isDisplayed()));
        BookmarkTestUtil.getRecyclerRowViewInteraction("Reading list", false)
                .check(matches(isDisplayed()));
    }

    // Checks the toolbar title against the given string.
    private void checkToolbarTitleMatches(String text) {
        onView(allOf(isDescendantOfA(withId(R.id.action_bar)), withText(text)))
                .check(matches(isDisplayed()));
    }
}
