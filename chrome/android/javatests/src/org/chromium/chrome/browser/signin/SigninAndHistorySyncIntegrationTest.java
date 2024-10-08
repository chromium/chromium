// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.StringContains.containsString;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
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

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator.HistoryOptInMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.ViewUtils;

/** Integration tests for the sign-in and history sync opt-in flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@EnableFeatures({
    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
    ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP
})
public class SigninAndHistorySyncIntegrationTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    /*
     * The tested SigninAndHistorySyncActivity will be on top of the BlankUiTestActivity.
     * Given the bottom sheet dismissal without sign-in action closes SigninAndHistorySyncActivity,
     * using BlankUiTestActivity allows to:
     *   - avoid `NoActivityResumedException` during the backpress action;
     *   - better approximate the normal behavior of the new sign-in flow which is always opened
     *     on top of another activity.
     */
    @Rule(order = 1)
    public final BaseActivityTestRule<BlankUiTestActivity> mBlankActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule(order = 2)
    public final BaseActivityTestRule<SigninAndHistorySyncActivity> mActivityTestRule =
            new BaseActivityTestRule(SigninAndHistorySyncActivity.class);

    private SigninAndHistorySyncActivity mActivity;
    private @SigninAccessPoint int mSigninAccessPoint = SigninAccessPoint.NTP_SIGNED_OUT_ICON;

    @Mock private HistorySyncHelper mHistorySyncHelperMock;
    @Mock private DeviceLockActivityLauncherImpl mDeviceLockActivityLauncher;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
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

        // Skip device lock UI on automotive.
        DeviceLockActivityLauncherImpl.setInstanceForTesting(mDeviceLockActivityLauncher);
        doAnswer(
                        invocation -> {
                            IntentCallback callback = (IntentCallback) invocation.getArguments()[4];
                            callback.onIntentCompleted(Activity.RESULT_OK, null);
                            return null;
                        })
                .when(mDeviceLockActivityLauncher)
                .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_requiredHistoryOptIn() {
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_historySyncDeclinedOften_requiredHistoryOptIn() {
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_historySyncSupressed() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_historySyncDeclinedOften_optionalHistoryOptIn() {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testWithExistingSignedInAccount_onlyShowsHistoryOptIn() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        // The footer should show the email of the signed in account.
        onView(withId(R.id.history_sync_footer))
                .inRoot(isDialog())
                .check(
                        matches(
                                allOf(
                                        isDisplayed(),
                                        withText(
                                                containsString(
                                                        AccountManagerTestRule.AADC_ADULT_ACCOUNT
                                                                .getEmail())))));

        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ true);
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_optOutHistorySync() {
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        // Verify that the history opt-in dialog is shown and decline.
        onViewWaiting(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
        // The user has just signed in, so the footer shouldn't show the email.
        onViewWaiting(withId(R.id.history_sync_footer))
                .inRoot(isDialog())
                .check(
                        matches(
                                allOf(
                                        isDisplayed(),
                                        not(
                                                withText(
                                                        containsString(
                                                                AccountManagerTestRule
                                                                        .AADC_ADULT_ACCOUNT
                                                                        .getEmail()))))));

        onView(allOf(withId(R.id.button_secondary), isCompletelyDisplayed())).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_optionalHistoryOptIn() {
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_noHistoryOptIn() {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.TEST_ACCOUNT_1);

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READING_LIST_ENABLE_SYNC_TRANSPORT_MODE_UPON_SIGNIN)
    public void testWithExistingAccount_signinIn_turnsOnBookmarksAndReadingList() {
        // Sign-in, toggle bookmarks and reading list off, then sign out.
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        disableBookmarksAndReadingList();
        mSigninTestRule.signOut();

        // Override the access point to test bookmarks-specific behavior.
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        verifyCollapsedBottomSheetAndSignin(AccountManagerTestRule.TEST_ACCOUNT_1);

        // Verify that bookmarks and reading list were enabled.
        SyncTestUtil.waitForBookmarksAndReadingListEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_dismissCollapsedBottomSheet_backPress_fromBookmarks() {
        // The new sign-in flow contains behaviors specific to the bookmark access point (enabling
        // bookmark & reading list sync after successful sign-in) therefore the access point is
        // overridden here to ensure correct dismissal behavior in this case.
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);
        // Verify that the default account bottom sheet is shown.
        onView(
                        allOf(
                                withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()));

        // Press on the back button.
        Espresso.pressBack();

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        assertFalse(SyncTestUtil.isBookmarksAndReadingListEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signInWithExpandedBottomSheet_noHistoryOptIn() {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        // Select an account on the shown expanded sign-in bottom-sheet.
        onView(
                        allOf(
                                withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(AccountManagerTestRule.TEST_ACCOUNT_1);
        if (BuildInfo.getInstance().isAutomotive) {
            verify(mDeviceLockActivityLauncher)
                    .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
        }

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify that history sync is not enabled.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signInWithAddedAccount_requiredHistoryOptIn() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.ACTIVITY_SURVIVED)
                        .build();

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        // Select "Add Account" on the shown expanded sign-in bottom-sheet.
        onView(allOf(withText(R.string.signin_add_account_to_device), isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_dismissBottomSheet_backPress() {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        // Verifies that the default account sign-in bottom-sheet is shown and select the account.
        onView(
                        allOf(
                                withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify that the account list is shown.
        onView(withId(R.id.account_picker_state_expanded)).check(matches(isDisplayed()));

        Espresso.pressBack();

        // Verify that the default account bottom sheet is shown.
        onView(
                        allOf(
                                withText(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed))))
                .check(matches(isDisplayed()));

        // Press on the back button.
        Espresso.pressBack();

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_dismissExpandedBottomSheet_backPress() {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mBlankActivityTestRule.launchActivity(null);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.OPTIONAL);

        // Verifies that the expanded sign-in bottom-sheet is shown.
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.account_picker_state_expanded), isCompletelyDisplayed()));

        Espresso.pressBack();

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithNoAccount_noSignIn() {
        launchActivity(
                NoAccountSigninMode.NO_SIGNIN,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.NONE);

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
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

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithNoAccount_bottomSheetSignin_requiredHistorySync() {
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);

        verifyNoAccountBottomSheetAndSignin();
        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ false);
    }

    @Test
    @MediumTest
    public void testWithNoAccount_bottomSheetSignin_requiredHistorySync_cancelAddAccount() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.CANCELLED,
                                State.ACTIVITY_SURVIVED)
                        .build();
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);
        // Start sign-in from the 0-account sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_no_account)),
                                isCompletelyDisplayed()))
                .perform(click());
        onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        onViewWaiting(
                allOf(
                        withId(R.id.account_picker_continue_as_button),
                        withParent(withId(R.id.account_picker_state_no_account)),
                        isCompletelyDisplayed()));
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testWithNoAccount_instantSignin_requiredHistorySync() {
        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Started", mSigninAccessPoint);
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.ACTIVITY_SURVIVED)
                        .build();

        launchActivity(
                NoAccountSigninMode.ADD_ACCOUNT,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);
        mSigninTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        acceptHistorySyncAndVerifyFlowCompletion(/* checkDialogRoot= */ true);
        signinStartedWatcher.assertExpected();
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testWithNoAccount_instantSignin_requiredHistorySync_cancelAddAccount() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.CANCELLED,
                                State.ACTIVITY_SURVIVED)
                        .build();
        launchActivity(
                NoAccountSigninMode.ADD_ACCOUNT,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistoryOptInMode.REQUIRED);
        onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        addAccountStateWatcher.assertExpected();
    }

    private void launchActivity(
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistoryOptInMode int historyOptInMode) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        Intent intent = SigninAndHistorySyncActivity.createIntent(
                ApplicationProvider.getApplicationContext(), bottomSheetStrings,
                noAccountSigninMode, withAccountSigninMode, historyOptInMode, mSigninAccessPoint);
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
    }

    private void verifyCollapsedBottomSheetAndSignin(CoreAccountInfo accountInfo) {
        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Started", mSigninAccessPoint);

        // Start sign-in from the collapsed sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        signinStartedWatcher.assertExpected();

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);
        if (BuildInfo.getInstance().isAutomotive) {
            verify(mDeviceLockActivityLauncher)
                    .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
        }
    }

    private void verifyNoAccountBottomSheetAndSignin() {
        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Started", mSigninAccessPoint);
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.ACTIVITY_SURVIVED)
                        .build();

        // Start sign-in from the 0-account sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_no_account)),
                                isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(
                AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        signinStartedWatcher.assertExpected();
        addAccountStateWatcher.assertExpected();

        mSigninTestRule.waitForSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        if (BuildInfo.getInstance().isAutomotive) {
            verify(mDeviceLockActivityLauncher)
                    .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
        }
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
            onViewWaiting(withId(R.id.button_primary), /* checkRootDialog= */ true)
                    .perform(click());
        } else {
            onViewWaiting(withId(R.id.history_sync_illustration)).check(matches(isDisplayed()));
            onViewWaiting(withId(R.id.button_primary)).perform(click());
        }

        // Verify history sync state.
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    private void disableBookmarksAndReadingList() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    syncService.setSelectedType(UserSelectableType.BOOKMARKS, false);
                    syncService.setSelectedType(UserSelectableType.READING_LIST, false);
                });
        assertFalse(SyncTestUtil.isBookmarksAndReadingListEnabled());
    }
}
