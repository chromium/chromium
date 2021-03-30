// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.browser.signin.ui.SigninActivityLauncher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Tests for the personalized signin promo on the Bookmarks page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class BookmarkPersonalizedSigninPromoTest {
    private final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    // As bookmarks need the fake AccountManagerFacade in AccountManagerTestRule,
    // BookmarkTestRule should be initialized after and destroyed before the
    // AccountManagerTestRule.
    @Rule
    public final RuleChain chain =
            RuleChain.outerRule(mAccountManagerTestRule).around(mBookmarkTestRule);

    private final SigninActivityLauncher mMockSigninActivityLauncher =
            mock(SigninActivityLauncher.class);

    @Before
    public void setUp() {
        BookmarkPromoHeader.forcePromoStateForTests(
                BookmarkPromoHeader.PromoState.PROMO_SIGNIN_PERSONALIZED);
        SigninActivityLauncherImpl.setLauncherForTest(mMockSigninActivityLauncher);
    }

    @After
    public void tearDown() {
        SigninActivityLauncherImpl.setLauncherForTest(null);
        BookmarkPromoHeader.forcePromoStateForTests(null);
    }

    @Test
    @MediumTest
    public void testSigninButtonDefaultAccount() {
        doNothing()
                .when(SigninActivityLauncherImpl.get())
                .launchActivityForPromoDefaultFlow(any(Context.class), anyInt(), anyString());
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
        onView(withId(R.id.signin_promo_signin_button)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoDefaultFlow(any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER), eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    public void testSigninButtonNotDefaultAccount() {
        doNothing()
                .when(SigninActivityLauncherImpl.get())
                .launchActivityForPromoChooseAccountFlow(any(Context.class), anyInt(), anyString());
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
        onView(withId(R.id.signin_promo_choose_account_button)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoChooseAccountFlow(any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER), eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    public void testSigninButtonNewAccount() {
        doNothing()
                .when(SigninActivityLauncherImpl.get())
                .launchActivityForPromoAddAccountFlow(any(Context.class), anyInt());
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
        onView(withId(R.id.signin_promo_signin_button)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoAddAccountFlow(
                        any(Activity.class), eq(SigninAccessPoint.BOOKMARK_MANAGER));
    }

    private void showBookmarkManagerAndCheckSigninPromoIsDisplayed() {
        mBookmarkTestRule.showBookmarkManager(sActivityTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }
}
