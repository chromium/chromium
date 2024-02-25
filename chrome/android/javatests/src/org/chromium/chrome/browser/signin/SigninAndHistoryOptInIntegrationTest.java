// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.content.Intent;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Integration tests for the sign-in and history sync opt-in flow. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class SigninAndHistoryOptInIntegrationTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    public final BaseActivityTestRule<SigninAndHistoryOptInActivity> mActivityTestRule =
            new BaseActivityTestRule(SigninAndHistoryOptInActivity.class);

    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    // Fake sign-in environment needs to be destroyed after the activity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    private Activity mActivity;

    private SigninAndHistoryOptInCoordinator mCoordinator;

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO(crbug.com/41493758): Handle the case where the UI is shown before
                    // the end of native initialization.
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    FirstRunStatus.setFirstRunFlowComplete(true);
                });
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_requiredHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);

        launchActivity(
                SigninAndHistoryOptInCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                SigninAndHistoryOptInCoordinator.HistoryOptInMode.REQUIRED);

        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify that the history opt-in dialog is shown.
        onView(withId(R.id.modal_dialog_view)).check(matches(isDisplayed()));

        // Verify signed-in state and flow completion.
        mSigninTestRule.waitForSignin(accountInfo);

        // TODO(crbug.com/41493758): Verify that history sync & MSBB are enabled, and the flow is
        // completed.
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_optionalHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        launchActivity(
                SigninAndHistoryOptInCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                SigninAndHistoryOptInCoordinator.HistoryOptInMode.OPTIONAL);

        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify that the history opt-in dialog is shown.
        onView(withId(R.id.modal_dialog_view)).check(matches(isDisplayed()));

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);

        // TODO(crbug.com/41493758): Verify that history sync is enabled and create test for
        // suppressed history prompt. Verify flow completion.
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_noHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        launchActivity(
                SigninAndHistoryOptInCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                SigninAndHistoryOptInCoordinator.HistoryOptInMode.NONE);

        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // TODO(crbug.com/41493758): Verify that history sync is not enabled.
    }

    @Test
    @MediumTest
    public void testWithNoAccount_noSignIn() {
        launchActivity(
                SigninAndHistoryOptInCoordinator.NoAccountSigninMode.NO_SIGNIN,
                SigninAndHistoryOptInCoordinator.HistoryOptInMode.NONE);

        // Verify that no account is signed-in.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // TODO(crbug.com/41493758): Verify that history sync is not enabled.
    }

    private void launchActivity(
            @SigninAndHistoryOptInCoordinator.NoAccountSigninMode int noAccountSigninMode,
            @SigninAndHistoryOptInCoordinator.HistoryOptInMode int historyOptInMode) {
        Intent intent =
                SigninAndHistoryOptInActivity.createIntent(
                        ContextUtils.getApplicationContext(),
                        noAccountSigninMode,
                        historyOptInMode,
                        SigninAccessPoint.NTP_SIGNED_OUT_ICON);
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
    }
}
