// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.mock;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.support.test.runner.lifecycle.Stage;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the class {@link SigninFirstRunFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninFirstRunFragmentTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";
    private static final String CHILD_EMAIL = "child.account@gmail.com";
    private static final String CHILD_FULL_NAME = "Test Child";

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade() {
                @Override
                public void checkChildAccountStatus(
                        Account account, ChildAccountStatusListener listener) {
                    listener.onStatusReady(account.name.equals(CHILD_EMAIL)
                                    ? ChildAccountStatus.REGULAR_CHILD
                                    : ChildAccountStatus.NOT_CHILD);
                }
            };

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade, new FakeAccountInfoService());

    @Rule
    public final ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private SigninFirstRunFragment mFragment;

    @Before
    public void setUp() {
        SigninCheckerProvider.setForTests(mock(SigninChecker.class));
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mFragment = new SigninFirstRunFragment();
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAccountDynamically() {
        launchActivityWithFragment();
        Assert.assertFalse(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown());
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));

        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testFragmentWithDefaultAccount() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        launchActivityWithFragment();

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testFragmentWhenChoosingAnotherAccount() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountManagerTestRule.addAccount(
                TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();
        onView(withText(TEST_EMAIL1)).perform(click());

        onView(withText(TEST_EMAIL2)).inRoot(isDialog()).perform(click());

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
    }

    @Test
    @MediumTest
    public void testFragmentWithSupervisedAccount() {
        mAccountManagerTestRule.addAccount(
                CHILD_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);

        launchActivityWithFragment();

        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        Assert.assertFalse(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account).isEnabled());
        onView(withText(CHILD_EMAIL)).check(matches(isDisplayed()));
        onView(withText(CHILD_FULL_NAME)).check(matches(isDisplayed()));
        final String continueAsText = mChromeActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, CHILD_FULL_NAME);
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAnotherAccount() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        onView(withText(TEST_EMAIL1)).perform(click());
        onView(withText(R.string.signin_add_account_to_device)).perform(click());

        Intent data = new Intent();
        data.putExtra(AccountManager.KEY_ACCOUNT_NAME, TEST_EMAIL2);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mFragment.onActivityResult(
                                SigninFirstRunFragment.ADD_ACCOUNT_REQUEST_CODE, Activity.RESULT_OK,
                                data));

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingDefaultAccount() {
        launchActivityWithFragment();
        onView(withText(R.string.signin_add_account_to_device)).perform(click());

        Intent data = new Intent();
        data.putExtra(AccountManager.KEY_ACCOUNT_NAME, TEST_EMAIL1);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mFragment.onActivityResult(
                                SigninFirstRunFragment.ADD_ACCOUNT_REQUEST_CODE, Activity.RESULT_OK,
                                data));

        checkFragmentWithSelectedAccount(TEST_EMAIL1, /* fullName= */ null, /* givenName= */ null);
    }

    private void checkFragmentWithSelectedAccount(String email, String fullName, String givenName) {
        CriteriaHelper.pollUiThread(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account)::isShown);
        final DisplayableProfileData profileData =
                new DisplayableProfileData(email, mock(Drawable.class), fullName, givenName);
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withText(email)).check(matches(isDisplayed()));
        if (fullName != null) {
            onView(withText(fullName)).check(matches(isDisplayed()));
        }
        final String continueAsText = mFragment.getString(
                R.string.signin_promo_continue_as, profileData.getGivenNameOrFullNameOrEmail());
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));
    }

    private void launchActivityWithFragment() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mChromeActivityTestRule.getActivity()
                    .getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, mFragment)
                    .commit();
        });
        ApplicationTestUtils.waitForActivityState(
                mChromeActivityTestRule.getActivity(), Stage.RESUMED);
    }
}