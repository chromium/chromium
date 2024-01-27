// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.ArgumentMatchers.matches;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.sync.SyncFeatureMap;

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
                        () -> BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile()));
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

    private void checkTopLevelAccountFoldersDisplayed() {
        checkToolbarTitleMatches("Bookmarks");
        onView(withText("In your Google Account")).check(matches(isDisplayed()));
        getRecyclerViewItem("Mobile bookmarks", true).check(matches(isDisplayed()));
        onView(withText("Only on this device")).check(matches(isDisplayed()));
        getRecyclerViewItem("Mobile bookmarks", false).check(matches(isDisplayed()));
        getRecyclerViewItem("Reading list", false).check(matches(isDisplayed()));
    }

    private ViewInteraction getRecyclerViewItem(String text, boolean isAccountBookmark) {
        return onView(getRecyclerItemMatcher(text, isAccountBookmark));
    }

    private Matcher<View> getRecyclerItemMatcher(String text, boolean isAccountBookmark) {
        ViewMatchers.Visibility visibility =
                isAccountBookmark ? ViewMatchers.Visibility.GONE : ViewMatchers.Visibility.VISIBLE;
        return allOf(
                withId(R.id.container),
                hasDescendant(withText(text)),
                hasDescendant(
                        allOf(
                                withId(R.id.local_bookmark_image),
                                withEffectiveVisibility(visibility))));
    }

    // Checks the toolbar title against the given string.
    private void checkToolbarTitleMatches(String text) {
        onView(allOf(isDescendantOfA(withId(R.id.action_bar)), withText(text)))
                .check(matches(isDisplayed()));
    }
}
