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
import static org.junit.Assert.assertFalse;
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
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.HistoryOptInMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.NoAccountSigninMode;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Integration tests for the sign-in and history sync opt-in flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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

        launchActivity(NoAccountSigninMode.BOTTOM_SHEET, HistoryOptInMode.REQUIRED);

        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(withId(R.id.button_primary)).perform(click());

        // Verify history sync state.
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_optOutHistorySync() {
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

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);

        // Verify that the history opt-in dialog is shown and decline.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(withId(R.id.button_secondary)).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_optionalHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        launchActivity(NoAccountSigninMode.BOTTOM_SHEET, HistoryOptInMode.OPTIONAL);

        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(withId(R.id.button_primary)).perform(click());

        // Verify history sync state.
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_noHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        launchActivity(NoAccountSigninMode.BOTTOM_SHEET, HistoryOptInMode.NONE);

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

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_refuseSignIn_noHistoryOptIn() {
        mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        launchActivity(
                SigninAndHistoryOptInCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                SigninAndHistoryOptInCoordinator.HistoryOptInMode.REQUIRED);

        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_dismiss_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()))
                .perform(click());

        // Verify that no account is signed-in and that history sync is not enabled.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());

        // TODO(crbug.com/41493758): Verify flow completion.
    }

    @Test
    @MediumTest
    public void testWithNoAccount_noSignIn() {
        launchActivity(NoAccountSigninMode.NO_SIGNIN, HistoryOptInMode.NONE);

        // Verify that no account is signed-in.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    private void launchActivity(
            @NoAccountSigninMode int noAccountSigninMode, @HistoryOptInMode int historyOptInMode) {
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
