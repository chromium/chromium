// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.ui.SigninPromoController;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests different scenarios when the bookmark personalized signin promo is not shown.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.DisableFeatures({ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD,
        ChromeFeatureList.INTEREST_FEED_V2})
public class BookmarkPersonalizedSigninPromoDismissTest {
    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    // As bookmarks need the fake AccountManagerFacade in SyncTestRule,
    // BookmarkTestRule should be initialized after and destroyed before the
    // SyncTestRule.
    @Rule
    public final RuleChain chain = RuleChain.outerRule(mSyncTestRule).around(mBookmarkTestRule);

    @Before
    public void setUp() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(null);
        BookmarkPromoHeader.setPrefPersonalizedSigninPromoDeclinedForTests(false);
        SigninPromoController.setSigninPromoImpressionsCountBookmarksForTests(0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkModel bookmarkModel = new BookmarkModel(Profile.fromWebContents(
                    mSyncTestRule.getActivity().getActivityTab().getWebContents()));
            bookmarkModel.loadFakePartnerBookmarkShimForTesting();
        });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    @After
    public void tearDown() {
        SigninPromoController.setSigninPromoImpressionsCountBookmarksForTests(0);
        BookmarkPromoHeader.setPrefPersonalizedSigninPromoDeclinedForTests(false);
    }

    @Test
    @MediumTest
    public void testPromoNotShownAfterBeingDismissed() {
        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onViewWaiting(allOf(withId(R.id.signin_promo_view_container), isDisplayed()));
        onView(withId(R.id.signin_promo_close_button)).perform(click());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        closeBookmarkManager();
        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    private void closeBookmarkManager() {
        if (mSyncTestRule.getActivity().isTablet()) {
            ChromeTabbedActivity chromeTabbedActivity =
                    (ChromeTabbedActivity) mSyncTestRule.getActivity();
            ChromeTabUtils.closeCurrentTab(
                    InstrumentationRegistry.getInstrumentation(), chromeTabbedActivity);
        } else {
            onView(withId(R.id.close_menu_id)).perform(click());
        }
    }

    @Test
    @MediumTest
    public void testPromoNotExistWhenImpressionLimitReached() {
        SigninPromoController.setSigninPromoImpressionsCountBookmarksForTests(
                SigninPromoController.getMaxImpressionsBookmarksForTests());
        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testPromoImpressionCountIncrementAfterDisplayingSigninPromo() {
        assertEquals(0, SigninPromoController.getSigninPromoImpressionsCountBookmarks());
        mBookmarkTestRule.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        assertEquals(1, SigninPromoController.getSigninPromoImpressionsCountBookmarks());
    }
}
