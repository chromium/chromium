// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeTabUtils;

/**
 * Tests different scenarios when the bookmark personalized signin promo is not shown.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BookmarkPersonalizedSigninPromoDismissTest {
    @Rule
    public final ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() {
        BookmarkPromoHeader.forcePromoStateForTests(null);
        BookmarkPromoHeader.setPrefPersonalizedSigninPromoDeclinedForTests(false);
        SigninPromoController.setSigninPromoImpressionsCountBookmarksForTests(0);
        mActivityTestRule.startMainActivityFromLauncher();
    }

    @After
    public void tearDown() {
        SigninPromoController.setSigninPromoImpressionsCountBookmarksForTests(0);
        BookmarkPromoHeader.setPrefPersonalizedSigninPromoDeclinedForTests(false);
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1020068")
    public void testPromoNotShownAfterBeingDismissed() {
        BookmarkTestUtil.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_close_button)).perform(click());
        onView(withId(R.id.signin_promo_view_container))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        closeBookmarkManager();
        BookmarkTestUtil.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    private void closeBookmarkManager() {
        if (mActivityTestRule.getActivity().isTablet()) {
            ChromeTabbedActivity chromeTabbedActivity =
                    (ChromeTabbedActivity) mActivityTestRule.getActivity();
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
        BookmarkTestUtil.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1020069")
    public void testPromoImpressionCountIncrementAfterDisplayingSigninPromo() {
        assertEquals(0, SigninPromoController.getSigninPromoImpressionsCountBookmarksForTests());
        BookmarkTestUtil.showBookmarkManager(mActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        assertEquals(1, SigninPromoController.getSigninPromoImpressionsCountBookmarksForTests());
    }
}
