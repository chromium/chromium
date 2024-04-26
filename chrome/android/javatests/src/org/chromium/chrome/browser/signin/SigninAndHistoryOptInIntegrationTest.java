// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.HistoryOptInMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.ViewUtils;

/** Integration tests for the sign-in and history sync opt-in flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
// TODO(crbug.com/41496906): Tests temporarily disabled for automotive. They should be
// re-enabled once the new sign-in flow is implemented for automotive.
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class SigninAndHistoryOptInIntegrationTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    /*
     * The tested SigninAndHistoryOptInActivity will be on top of the BlankUiTestActivity.
     * Given the bottom sheet dismissal without sign-in action closes SigninAndHistoryOptInActivity,
     * using BlankUiTestActivity allows to:
     *   - avoid `NoActivityResumedException` during the backpress action;
     *   - better approximate the normal behavior of the new sign-in flow which is always opened
     *     on top of another activity.
     */
    @Rule(order = 1)
    public final BaseActivityTestRule<BlankUiTestActivity> mBlankActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule(order = 2)
    public final BaseActivityTestRule<SigninAndHistoryOptInActivity> mActivityTestRule =
            new BaseActivityTestRule(SigninAndHistoryOptInActivity.class);

    private SigninAndHistoryOptInActivity mActivity;
    private @SigninAccessPoint int mSigninAccessPoint = SigninAccessPoint.NTP_SIGNED_OUT_ICON;

    @Mock private HistorySyncHelper mHistorySyncHelperMock;

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO(crbug.com/41493758): Handle the case where the UI is shown before
                    // the end of native initialization.
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    FirstRunStatus.setFirstRunFlowComplete(true);
                });
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);
        when(mHistorySyncHelperMock.didAlreadyOptIn()).thenReturn(false);
        when(mHistorySyncHelperMock.isHistorySyncDisabledByCustodian()).thenReturn(false);
        when(mHistorySyncHelperMock.isHistorySyncDisabledByPolicy()).thenReturn(false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_requiredHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(accountInfo);
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_historySyncSupressed() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);
        mSigninTestRule.addAccountAndWaitForSeeding(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.TEST_ACCOUNT_1);

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testWithExistingSignedInAccount_onlyShowsHistoryOptIn() {
        mSigninTestRule.addTestAccountThenSignin();

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ true);
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_optOutHistorySync() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(accountInfo);

        // Verify that the history opt-in dialog is shown and decline.
        onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.button_secondary), isCompletelyDisplayed())).perform(click());

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

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        verifyCollapsedBottomSheetAndSignin(accountInfo);
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_noHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        verifyCollapsedBottomSheetAndSignin(accountInfo);

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @EnableFeatures(SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE)
    public void testWithExistingAccount_dismissCollapsedBottomSheet_backPress_fromBookmarks() {
        // The new sign-in flow contains behaviors specific to the bookmark access point (enabling
        // bookmark & reading list sync after successful sign-in) therefore the access point is
        // overridden here to ensure correct dismissal behavior in this case.
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);
        // Verify that the default account bottom sheet is shown.
        onView(
                        allOf(
                                withText(SigninTestRule.TEST_ACCOUNT_EMAIL),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()));

        // Press on the back button.
        Espresso.pressBack();

        verifySigninCancelled();
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signInWithExpandedBottomSheet_noHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        // Verifies that the expanded sign-in bottom-sheet is shown, and select an account.
        onView(
                        allOf(
                                withText(SigninTestRule.TEST_ACCOUNT_EMAIL),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify that history sync is not enabled.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    // Enabling SEED_ACCOUNTS_REVAMP to avoid failure due to the deprecated seed account flow. See
    // https://crbug.com/336371182.
    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.SEED_ACCOUNTS_REVAMP)
    public void testWithExistingAccount_signInWithAddedAccount_requiredHistoryOptIn() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, accountInfo.getEmail());

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        // Verifies that the expanded sign-in bottom-sheet is shown, and select add account.
        onView(allOf(withText(R.string.signin_add_account_to_device), isCompletelyDisplayed()))
                .perform(click());
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_dismissBottomSheet_backPress() {
        mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        // Verifies that the default account sign-in bottom-sheet is shown and select the account.
        onView(
                        allOf(
                                withText(SigninTestRule.TEST_ACCOUNT_EMAIL),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify that the account list is shown.
        onView(withId(R.id.account_picker_state_expanded)).check(matches(isDisplayed()));

        Espresso.pressBack();

        // Verify that the default account bottom sheet is shown.
        onView(
                        allOf(
                                withText(SigninTestRule.TEST_ACCOUNT_EMAIL),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()));

        // Press on the back button.
        Espresso.pressBack();

        verifySigninCancelled();
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_dismissExpandedBottomSheet_backPress() {
        mSigninTestRule.addAccountAndWaitForSeeding(SigninTestRule.TEST_ACCOUNT_EMAIL);
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        // Verifies that the expanded sign-in bottom-sheet is shown.
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.account_picker_state_expanded), isCompletelyDisplayed()));

        Espresso.pressBack();

        verifySigninCancelled();
    }

    @Test
    @MediumTest
    public void testWithNoAccount_noSignIn() {
        launchActivity(
                NoAccountSigninMode.NO_SIGNIN,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        verifySigninCancelled();
    }

    @Test
    @MediumTest
    public void testWithNoAccount_dismissBottomSheet_backPress() {
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        // Verifies that the no account sign-in bottom-sheet is shown.
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.account_picker_state_no_account), isCompletelyDisplayed()));

        Espresso.pressBack();

        verifySigninCancelled();
    }

    // Enabling SEED_ACCOUNTS_REVAMP to avoid failure due to the deprecated seed account flow. See
    // https://crbug.com/336371182.
    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.SEED_ACCOUNTS_REVAMP)
    public void testWithNoAccount_bottomSheetSignin_requiredHistorySync() {
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyNoAccountBottomSheetAndSignin();
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    // Enabling SEED_ACCOUNTS_REVAMP to avoid failure due to the deprecated seed account flow. See
    // https://crbug.com/336371182.
    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.SEED_ACCOUNTS_REVAMP)
    public void testWithNoAccount_instantSignin_requiredHistorySync() {
        CoreAccountInfo accountInfo = AccountManagerTestRule.TEST_ACCOUNT_1;
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, accountInfo.getEmail());

        launchActivity(
                NoAccountSigninMode.ADD_ACCOUNT,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ true);
    }

    @Test
    @MediumTest
    public void testWithNoAccount_instantSignin_requiredHistorySync_cancelAddAccount() {
        CoreAccountInfo accountInfo = AccountManagerTestRule.TEST_ACCOUNT_1;
        mSigninTestRule.setResultForNextAddAccountFlow(
                Activity.RESULT_CANCELED, accountInfo.getEmail());

        launchActivity(
                NoAccountSigninMode.ADD_ACCOUNT,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifySigninCancelled();
    }

    private void launchActivity(
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistoryOptInMode int historyOptInMode) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(R.string.sign_in_to_chrome).build();
        Intent intent =
                SigninAndHistoryOptInActivity.createIntent(
                        ApplicationProvider.getApplicationContext(),
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        historyOptInMode,
                        mSigninAccessPoint);
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
    }

    private void verifyCollapsedBottomSheetAndSignin(CoreAccountInfo accountInfo) {
        // Verify that the collapsed sign-in bottom-sheet is shown, and start sign-in.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);
    }

    private void verifyNoAccountBottomSheetAndSignin() {
        CoreAccountInfo accountInfo = AccountManagerTestRule.TEST_ACCOUNT_1;
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, accountInfo.getEmail());
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_no_account)),
                                isCompletelyDisplayed()))
                .perform(click());

        mSigninTestRule.waitForSignin(accountInfo);
    }

    // `checkDialogRoot` should be set to true for tests that fail due to Espresso using the wrong
    // root view for dialogs on API30+ (Mostly when the dialog appears without the bottom sheet
    // being shown before).
    // See https://crbug.com/332025155.
    private void acceptHistorySyncAndVerifyFlowCompletion(boolean checkDialogRoot) {
        // Verify that the history opt-in dialog is shown and accept.
        if (checkDialogRoot) {
            onViewWaiting(withId(R.id.history_sync_illustration), /* checkRootDialog= */ true)
                    .check(matches(isDisplayed()));
            onViewWaiting(
                            allOf(withId(R.id.button_primary), isCompletelyDisplayed()),
                            /* checkRootDialog= */ true)
                    .perform(click());
        } else {
            onView(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
            onView(allOf(withId(R.id.button_primary), isCompletelyDisplayed())).perform(click());
        }

        // Verify history sync state.
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    // Verifies that the activity finishes, no account is signed in, and history sync is disabled.
    private void verifySigninCancelled() {
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }
}
