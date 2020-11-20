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

import static org.junit.Assert.assertEquals;

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
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
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
    private final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();
    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // Bookmarks need fake AccountManagerFacade. ChromeBrowserTestRule initializes fake
    // AccountManagerFacade as part of initializing AccountManagerTestRule inside it.
    // BookmarkTestRule should be initialized after and destroyed before the
    // ChromeBrowserTestRule.
    @Rule
    public final RuleChain chain =
            RuleChain.outerRule(mChromeBrowserTestRule).around(mBookmarkTestRule);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        BookmarkPromoHeader.forcePromoStateForTests(null);
        BookmarkPromoHeader.setPrefPersonalizedSigninPromoDeclinedForTests(false);
        SigninPromoController.setSigninPromoImpressionsCountBookmarksForTests(0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkModel bookmarkModel = new BookmarkModel(Profile.fromWebContents(
                    mActivityTestRule.getActivity().getActivityTab().getWebContents()));
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
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_close_button)).perform(click());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        closeBookmarkManager();
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    private void closeBookmarkManager() {
        if (mActivityTestRule.getActivity().isTablet()) {
            ChromeTabbedActivity chromeTabbedActivity = mActivityTestRule.getActivity();
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
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testPromoImpressionCountIncrementAfterDisplayingSigninPromo() {
        assertEquals(0, SigninPromoController.getSigninPromoImpressionsCountBookmarks());
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        assertEquals(1, SigninPromoController.getSigninPromoImpressionsCountBookmarks());
    }
}
