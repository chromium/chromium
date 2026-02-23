// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.transit.ViewFinder.waitForView;
import static org.chromium.base.test.util.ApplicationTestUtils.waitForActivityWithClass;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.transit.RootSpec;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator.Result;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.base.WindowAndroid.IntentCallback;
import org.chromium.ui.test.util.ViewUtils;

/** Integration tests for the sign-in and history sync opt-in flow. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@EnableFeatures({ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP})
// TODO(crbug.com/428056054): Test content is blocked by system UI on B+.
@DisableIf.Build(
        sdk_is_greater_than = Build.VERSION_CODES.VANILLA_ICE_CREAM,
        message = "crbug.com/428056054")
public class BottomSheetSigninAndHistorySyncIntegrationTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    /*
     * The tested SigninAndHistorySyncActivity will be on top of a blank ChromeTabbedActivity.
     * Given the bottom sheet dismissal without sign-in action closes SigninAndHistorySyncActivity,
     * using extra activity allows to:
     *   - avoid `NoActivityResumedException` during the backpress action;
     *   - better approximate the normal behavior of the new sign-in flow which is always opened
     *     on top of another activity.
     */
    @Rule(order = 1)
    public FreshCtaTransitTestRule mBaseActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule(order = 2)
    public final BaseActivityTestRule<SigninAndHistorySyncActivity> mActivityTestRule =
            new BaseActivityTestRule(SigninAndHistorySyncActivity.class);

    private SigninAndHistorySyncActivity mActivity;
    private BottomSheetSigninAndHistorySyncCoordinator mCoordinator;
    private PrefService mPrefService;
    private @SigninAccessPoint int mSigninAccessPoint = SigninAccessPoint.NTP_FEED_TOP_PROMO;

    @Mock private HistorySyncHelper mHistorySyncHelperMock;
    @Mock private DeviceLockActivityLauncherImpl mDeviceLockActivityLauncher;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator.Delegate mDelegate;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO(crbug.com/41493758): Handle the case where the UI is shown before
                    // the end of native initialization.
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    FirstRunStatus.setFirstRunFlowComplete(true);
                    mPrefService = UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                });
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);
        // Simulate the real HistorySyncHelper's interaction with SyncService to ensure
        // UserSelectableType.HISTORY and UserSelectableType.TABS are correctly set.
        doAnswer(
                        invocation -> {
                            boolean isTypeOn = invocation.getArgument(0);
                            SyncService syncService =
                                    SyncTestUtil.getSyncServiceForLastUsedProfile();
                            syncService.setSelectedType(UserSelectableType.HISTORY, isTypeOn);
                            syncService.setSelectedType(UserSelectableType.TABS, isTypeOn);
                            return null;
                        })
                .when(mHistorySyncHelperMock)
                .setHistoryAndTabsSync(anyBoolean());
        // By default, history sync dialog should be displayed and not yet disabled.
        when(mHistorySyncHelperMock.didAlreadyOptIn()).thenReturn(false);
        when(mHistorySyncHelperMock.isHistorySyncDisabledByCustodian()).thenReturn(false);
        when(mHistorySyncHelperMock.isHistorySyncDisabledByPolicy()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldDisplayHistorySync()).thenReturn(true);

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

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrefService.setBoolean(Pref.SIGNIN_ALLOWED, true);
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_requiredHistoryOptIn_legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.AADC_ADULT_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_requiredHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.AADC_ADULT_ACCOUNT);

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signInNotAllowed() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrefService.setBoolean(Pref.SIGNIN_ALLOWED, false);
                });

        launchSigninFlow(
                WithAccountSigninMode.SEAMLESS_SIGNIN,
                HistorySyncConfig.OptInMode.REQUIRED,
                TestAccounts.AADC_ADULT_ACCOUNT.getId());

        // This is a Toast, so need to use RootSpec.anyRoot().
        waitForView(
                withText(R.string.signin_account_picker_bottom_sheet_error_title),
                ViewElement.rootSpecOption(RootSpec.anyRoot()));
        verify(mDelegate, never()).onFlowComplete(any());
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithAadcMinorAccount_requiredHistoryOptIn_legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.AADC_MINOR_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithAadcMinorAccount_requiredHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.AADC_MINOR_ACCOUNT);

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void
            testWithExistingAccount_signIn_historySyncDeclinedOften_requiredHistoryOptIn_legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.AADC_ADULT_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_historySyncDeclinedOften_requiredHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.AADC_ADULT_ACCOUNT);

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_historySyncSupressed_legacy() {
        when(mHistorySyncHelperMock.shouldDisplayHistorySync()).thenReturn(false);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.ACCOUNT1);
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_historySyncSupressed() {
        when(mHistorySyncHelperMock.shouldDisplayHistorySync()).thenReturn(false);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.ACCOUNT1);

        // Verify that the history opt-in dialog is not shown. Wait for UI-related callbacks to be
        // executed while seamless sign-in asynchronously completes.
        verify(mHistorySyncHelperMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .recordHistorySyncNotShown(mSigninAccessPoint);
        // Verify that the flow completion callback is called.
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ true,
                                        /* hasOptedInHistorySync= */ false)));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void
            testWithExistingAccount_signIn_historySyncDeclinedOften_optionalHistoryOptIn_legacy() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.OPTIONAL);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.ACCOUNT1);
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_historySyncDeclinedOften_optionalHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.OPTIONAL, TestAccounts.ACCOUNT1);

        // Verify that the history opt-in dialog is not shown. Wait for UI-related callbacks to be
        // executed while seamless sign-in asynchronously completes.
        verify(mHistorySyncHelperMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .recordHistorySyncNotShown(mSigninAccessPoint);
        // Verify that the flow completion callback is called.
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ true,
                                        /* hasOptedInHistorySync= */ false)));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.FORCE_HISTORY_OPT_IN_SCREEN)
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void
            testWithExistingAccount_signIn_historySyncDeclinedOften_forceHistoryOptInScreen_legacy() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.OPTIONAL);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.ACCOUNT1);
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        SigninFeatures.FORCE_HISTORY_OPT_IN_SCREEN,
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN
    })
    public void testWithExistingAccount_signIn_historySyncDeclinedOften_forceHistoryOptInScreen() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.OPTIONAL, TestAccounts.ACCOUNT1);

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingSignedInAccount_onlyShowsHistoryOptIn_legacy() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.AADC_ADULT_ACCOUNT);
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        // The footer should show "You are signed in as..." with the email of the signed in account.
        var expectedEmail = TestAccounts.AADC_ADULT_ACCOUNT.getEmail();
        onView(withId(R.id.history_sync_footer))
                .inRoot(isDialog())
                .check(matches(allOf(isDisplayed(), withText(containsString(expectedEmail)))));

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ false);
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingSignedInAccount_onlyShowsHistoryOptIn() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.AADC_ADULT_ACCOUNT);
        launchSigninFlow(
                WithAccountSigninMode.SEAMLESS_SIGNIN,
                HistorySyncConfig.OptInMode.REQUIRED,
                TestAccounts.AADC_ADULT_ACCOUNT.getId());

        // The footer should show "You are signed in as..." with the email of the signed in account.
        var expectedEmail = TestAccounts.AADC_ADULT_ACCOUNT.getEmail();
        onView(withId(R.id.history_sync_footer))
                .inRoot(isDialog())
                .check(matches(allOf(isDisplayed(), withText(containsString(expectedEmail)))));

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ false);
        assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_optOutHistorySync_legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.AADC_ADULT_ACCOUNT);

        // Verify that the history opt-in dialog is shown and decline.
        waitForView(withId(R.id.history_sync_illustration));
        // The user has just signed in, so the footer shouldn't show the email.
        var email = TestAccounts.AADC_ADULT_ACCOUNT.getEmail();
        onView(withId(R.id.history_sync_footer))
                .inRoot(isDialog())
                .check(matches(allOf(isDisplayed(), not(withText(containsString(email))))));

        // Dismiss history sync.
        waitForView(withId(R.id.history_sync_illustration));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        // Should signout on decline.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_optOutHistorySync() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.AADC_ADULT_ACCOUNT);

        // Verify that the history opt-in dialog is shown and decline.
        waitForView(withId(R.id.history_sync_illustration));
        // The user has just signed in, so the footer shouldn't show the email.
        var email = TestAccounts.AADC_ADULT_ACCOUNT.getEmail();
        onView(withId(R.id.history_sync_footer))
                .inRoot(isDialog())
                .check(matches(allOf(isDisplayed(), not(withText(containsString(email))))));

        // Dismiss history sync.
        waitForView(withId(R.id.history_sync_illustration));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        verifyHistorySyncDialogDismissed();

        // Verify that the flow completion callback is called,
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ false,
                                        /* hasOptedInHistorySync= */ false)));

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        // Should signout on decline.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testBackPressHistorySync_legacy() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        // Add another activity in the back stack
        mBaseActivityTestRule.startOnBlankPage();

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.ACCOUNT1);

        // Verify that the history opt-in dialog is shown.
        waitForView(withId(R.id.history_sync_illustration));

        Espresso.pressBack();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        // Should signout on decline.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testBackPressHistorySync() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.ACCOUNT1);

        // Verify that the history opt-in dialog is shown.
        waitForView(withId(R.id.history_sync_illustration));

        Espresso.pressBack();

        verifyHistorySyncDialogDismissed();

        // Verify that the flow completion callback is called.
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ false,
                                        /* hasOptedInHistorySync= */ false)));

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        // Should signout on decline.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithAadcMinorAccount_signIn_optOutHistorySync_legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.AADC_MINOR_ACCOUNT);

        // Verify that the history opt-in dialog is shown and decline.
        waitForView(withId(R.id.history_sync_illustration));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithAadcMinorAccount_signIn_optOutHistorySync() {
        mSigninTestRule.addAccount(TestAccounts.AADC_MINOR_ACCOUNT);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.REQUIRED, TestAccounts.AADC_MINOR_ACCOUNT);

        // Verify that the history opt-in dialog is shown and decline.
        waitForView(withId(R.id.history_sync_illustration));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        verifyHistorySyncDialogDismissed();

        // Verify that the flow completion callback is called.
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ false,
                                        /* hasOptedInHistorySync= */ false)));

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        // Should signout on decline.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithManagedAccount_signIn_showsManagementNotice_legacy() {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.NONE);

        // Start sign-in from the collapsed sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        // The management notice should be displayed.
        waitForView(withText(R.string.sign_in_managed_account));
        onView(allOf(withText(R.string.continue_button), isCompletelyDisplayed())).perform(click());

        if (DeviceInfo.isAutomotive()) {
            verify(mDeviceLockActivityLauncher)
                    .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
        }

        // Verify that the activity is finished before checking sign-in - since the default poll
        // delay is too short for this flow.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        mSigninTestRule.waitForSignin(TestAccounts.MANAGED_ACCOUNT);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithManagedAccount_signIn_showsManagementNotice() {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        launchSigninFlow(
                WithAccountSigninMode.SEAMLESS_SIGNIN,
                HistorySyncConfig.OptInMode.NONE,
                TestAccounts.MANAGED_ACCOUNT.getId());

        // The management notice should be displayed.
        waitForView(withText(R.string.sign_in_managed_account));
        onView(allOf(withText(R.string.continue_button), isCompletelyDisplayed())).perform(click());

        mSigninTestRule.waitForSignin(TestAccounts.MANAGED_ACCOUNT);
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ true,
                                        /* hasOptedInHistorySync= */ false)));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_optionalHistoryOptIn_legacy() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.OPTIONAL);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.AADC_ADULT_ACCOUNT);
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_optionalHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.AADC_ADULT_ACCOUNT);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.OPTIONAL, TestAccounts.AADC_ADULT_ACCOUNT);

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_noHistoryOptIn_legacy() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.NONE);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.ACCOUNT1);

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signIn_noHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.NONE, TestAccounts.ACCOUNT1);

        // Verify that the flow completion callback is called.
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ true,
                                        /* hasOptedInHistorySync= */ false)));

        // Verify history sync state.
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signinIn_turnsOnBookmarksAndReadingList_legacy() {
        // Sign-in, toggle bookmarks and reading list off, then sign out.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingList();
        mSigninTestRule.signOut();

        // Override the access point to test bookmarks-specific behavior.
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.NONE);

        verifyCollapsedBottomSheetAndSignin(TestAccounts.ACCOUNT1);

        // Verify that bookmarks and reading list were enabled.
        SyncTestUtil.waitForBookmarksAndReadingListEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signinIn_turnsOnBookmarksAndReadingList() {
        // Sign-in, toggle bookmarks and reading list off, then sign out.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        disableBookmarksAndReadingList();
        mSigninTestRule.signOut();

        // Override the access point to test bookmarks-specific behavior.
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.NONE, TestAccounts.ACCOUNT1);

        // Verify that bookmarks and reading list were enabled.
        SyncTestUtil.waitForBookmarksAndReadingListEnabled();

        // Verify that the flow completion callback is called.
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ true,
                                        /* hasOptedInHistorySync= */ false)));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_dismissCollapsedBottomSheet_backPress_fromBookmarks() {
        // The new sign-in flow contains behaviors specific to the bookmark access point (enabling
        // bookmark & reading list sync after successful sign-in) therefore the access point is
        // overridden here to ensure correct dismissal behavior in this case.
        mSigninAccessPoint = SigninAccessPoint.BOOKMARK_MANAGER;
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mBaseActivityTestRule.startOnBlankPage();
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);
        // Verify that the default account bottom sheet is shown.
        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
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
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signInWithExpandedBottomSheet_noHistoryOptIn() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.NONE);

        // Select an account on the shown expanded sign-in bottom-sheet.
        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
                                isDescendantOfA(withId(R.id.account_picker_state_expanded)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(TestAccounts.ACCOUNT1);
        if (DeviceInfo.isAutomotive()) {
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
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signInWithAddedAccount_requiredHistoryOptIn_legacy() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.ACTIVITY_SURVIVED)
                        .build();

        // User clicked "Choose another account"
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        // Select "Add Account to device" on the shown expanded sign-in bottom-sheet.
        onView(allOf(withText(R.string.signin_add_account_to_device), isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(TestAccounts.AADC_ADULT_ACCOUNT);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signInWithAddedAccount_activityKilled_legacy() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.ACTIVITY_DESTROYED)
                        .build();

        // User clicked "Choose another account"
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        // Select "Add Account to device" on the shown expanded sign-in bottom-sheet.
        onView(allOf(withText(R.string.signin_add_account_to_device), isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(TestAccounts.AADC_ADULT_ACCOUNT);

        // Recreate base activity then confirm account addition.
        SigninAndHistorySyncActivity activity =
                waitForActivityWithClass(
                        mActivity.getClass(), Stage.CREATED, () -> mActivity.recreate());
        mActivityTestRule.setActivity(activity);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signInWithAddedAccount_activityKilled() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED)
                        .build();

        // User clicked "Choose another account"
        launchSigninFlow(
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED,
                TestAccounts.ACCOUNT1.getId());

        // Select "Add Account to device" on the shown expanded sign-in bottom-sheet.
        onView(allOf(withText(R.string.signin_add_account_to_device), isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(TestAccounts.AADC_ADULT_ACCOUNT);

        // Recreate base activity then confirm account addition.
        mBaseActivityTestRule.recreateActivity();
        createSigninCoordinator();

        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_signInWithAddedAccount_requiredHistoryOptIn() {
        HistogramWatcher addAccountStateWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED)
                        .build();

        // User clicked "Choose another account"
        launchSigninFlow(
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED,
                TestAccounts.ACCOUNT1.getId());

        // Select "Add Account to device" on the shown expanded sign-in bottom-sheet.
        onView(allOf(withText(R.string.signin_add_account_to_device), isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(TestAccounts.AADC_ADULT_ACCOUNT);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
        addAccountStateWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_dismissBottomSheet_backPress() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mBaseActivityTestRule.startOnBlankPage();
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.OPTIONAL);

        // Verifies that the default account sign-in bottom-sheet is shown and select the account.
        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
                                isDescendantOfA(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Verify that the account list is shown.
        onView(withId(R.id.account_picker_state_expanded)).check(matches(isDisplayed()));

        Espresso.pressBack();

        // Verify that the default account bottom sheet is shown.
        onView(
                        allOf(
                                withText(TestAccounts.ACCOUNT1.getFullName()),
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
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithExistingAccount_dismissExpandedBottomSheet_backPress() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mBaseActivityTestRule.startOnBlankPage();
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.OPTIONAL);

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
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithNoAccount_noSignIn() {
        launchActivity(
                NoAccountSigninMode.NO_SIGNIN,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.NONE);

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithNoAccount_dismissBottomSheet_backPress() {
        mBaseActivityTestRule.startOnBlankPage();
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.OPTIONAL);

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
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testWithNoAccount_bottomSheetSignin_requiredHistorySync() {
        launchActivity(
                NoAccountSigninMode.BOTTOM_SHEET,
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                HistorySyncConfig.OptInMode.REQUIRED);

        verifyNoAccountBottomSheetAndSignin();
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ true);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
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
                HistorySyncConfig.OptInMode.REQUIRED);
        // Start sign-in from the 0-account sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_no_account)),
                                isCompletelyDisplayed()))
                .perform(click());
        onViewWaiting(SigninTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

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
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testHistorySyncStrings_legacy() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder("Title").build();
        // Create a config using sign-in strings for the history sync screen to test customization.
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                bottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.REQUIRED,
                                "Title",
                                "Subtitle")
                        .build();
        Intent intent =
                SigninAndHistorySyncActivity.createIntent(
                        ApplicationProvider.getApplicationContext(), config, mSigninAccessPoint);
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();

        // Start sign-in from the collapsed bottom-sheet.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        // Wait for the history opt-in dialog and verify the custom strings.
        waitForView(withId(R.id.history_sync_illustration));
        onView(allOf(withId(R.id.history_sync_title), withText("Title")))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.history_sync_subtitle), withText("Subtitle")))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testHistorySyncStrings() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder("Title").build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                bottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.SEAMLESS_SIGNIN,
                                HistorySyncConfig.OptInMode.REQUIRED,
                                "Title",
                                "Subtitle")
                        .useSeamlessWithAccountSignin(TestAccounts.ACCOUNT1.getId())
                        .build();

        mBaseActivityTestRule.startOnBlankPage();
        createSigninCoordinator();
        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.startSigninFlow(config));

        // Verify seamless signin finished.
        mSigninTestRule.waitForSignin(TestAccounts.ACCOUNT1);

        // Wait for the history opt-in dialog and verify the custom strings.
        waitForView(withId(R.id.history_sync_illustration));
        onView(allOf(withId(R.id.history_sync_title), withText("Title")))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.history_sync_subtitle), withText("Subtitle")))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSeamlessSignin() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchSeamlessSigninAndVerifySignedIn(
                HistorySyncConfig.OptInMode.NONE, TestAccounts.ACCOUNT1);

        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(
                        eq(
                                new Result(
                                        /* hasSignedIn= */ true,
                                        /* hasOptedInHistorySync= */ false)));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSeamlessSigninWithAccountNotOnDevice() {
        launchSigninFlow(
                WithAccountSigninMode.SEAMLESS_SIGNIN,
                HistorySyncConfig.OptInMode.NONE,
                TestAccounts.ACCOUNT1.getId());

        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(eq(Result.aborted()));
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testIncognitoProfileCannotStartSigninFlow() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder("Title").build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                bottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.SEAMLESS_SIGNIN,
                                HistorySyncConfig.OptInMode.REQUIRED,
                                "Title",
                                "Subtitle")
                        .useSeamlessWithAccountSignin(TestAccounts.ACCOUNT1.getId())
                        .build();

        mBaseActivityTestRule.startOnBlankPage();
        ChromeTabbedActivity baseActivity = mBaseActivityTestRule.getActivity();
        Profile incognitoProfile =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                baseActivity
                                        .getProfileProviderSupplier()
                                        .get()
                                        .getOrCreateOffTheRecordProfile());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OneshotSupplierImpl<Profile> incognitoProfileSupplier =
                            new OneshotSupplierImpl<>();
                    incognitoProfileSupplier.set(incognitoProfile);
                    mCoordinator =
                            BottomSheetSigninAndHistorySyncCoordinator
                                    .createAndObserveAddAccountResult(
                                            baseActivity.getWindowAndroid(),
                                            /* activity= */ baseActivity,
                                            /* activityResultTracker= */ baseActivity
                                                    .getActivityResultTracker(),
                                            /* delegate= */ mDelegate,
                                            DeviceLockActivityLauncherImpl.get(),
                                            incognitoProfileSupplier,
                                            this::getBottomSheetController,
                                            baseActivity.getModalDialogManagerSupplier(),
                                            baseActivity.getSnackbarManager(),
                                            mSigninAccessPoint);
                    Assert.assertThrows(
                            IllegalStateException.class,
                            () -> mCoordinator.startSigninFlow(config));
                });

        verify(mDelegate, never()).onFlowComplete(any());
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testStartSigninFlow_afterAnotherSigninFlow_didShowSigninStepIsReset() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mBaseActivityTestRule.startOnBlankPage();
        createSigninCoordinator();

        // Start flow 1 with CHOOSE_ACCOUNT_BOTTOM_SHEET.
        // This sets mDidShowSigninStep = true.
        final BottomSheetSigninAndHistorySyncConfig config =
                createConfig(
                        NoAccountSigninMode.BOTTOM_SHEET,
                        WithAccountSigninMode.CHOOSE_ACCOUNT_BOTTOM_SHEET,
                        HistorySyncConfig.OptInMode.REQUIRED,
                        null);
        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.startSigninFlow(config));

        // Verify that the expanded sign-in bottom-sheet is shown.
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.account_picker_state_expanded), isCompletelyDisplayed()));

        // Cancel flow 1. This should call resetSigninFlow().
        Espresso.pressBack();
        verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onFlowComplete(eq(Result.aborted()));

        // Sign in manually so that the next flow skips sign-in.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Start flow 2 with SEAMLESS_SIGNIN for the already signed-in account.
        // This flow should skip sign-in and go to history sync.
        final BottomSheetSigninAndHistorySyncConfig newConfig =
                createConfig(
                        NoAccountSigninMode.BOTTOM_SHEET,
                        WithAccountSigninMode.SEAMLESS_SIGNIN,
                        HistorySyncConfig.OptInMode.REQUIRED,
                        TestAccounts.ACCOUNT1.getId());
        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.startSigninFlow(newConfig));

        // Verify that in flow 2, mDidShowSigninStep is false, meaning the email IS shown in footer,
        // and the completion result doesn't state that a sign-in has been done.
        String expectedEmail = TestAccounts.ACCOUNT1.getEmail();
        onViewWaiting(withId(R.id.history_sync_footer))
                .check(matches(withText(containsString(expectedEmail))));
        acceptHistorySyncAndVerifyFlowCompletion(/* hasSignedIn= */ false);
    }

    private void launchSeamlessSigninAndVerifySignedIn(
            @HistorySyncConfig.OptInMode int historyOptInMode, CoreAccountInfo accountInfo) {
        HistogramWatcher signinHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Signin.SignIn.Started", mSigninAccessPoint)
                        .expectAnyRecord("Signin.SignIn.Timestamps.Other.ManagementStatusLoaded")
                        .expectAnyRecord("Signin.SignIn.Timestamps.Other.SigninCompleted")
                        .build();

        launchSigninFlow(
                WithAccountSigninMode.SEAMLESS_SIGNIN, historyOptInMode, accountInfo.getId());

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);
        signinHistogramWatcher.assertExpected();
    }

    private void launchSigninFlow(
            @WithAccountSigninMode int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            CoreAccountId accountId) {
        BottomSheetSigninAndHistorySyncConfig config =
                createConfig(
                        NoAccountSigninMode.BOTTOM_SHEET,
                        withAccountSigninMode,
                        historyOptInMode,
                        accountId);

        mBaseActivityTestRule.startOnBlankPage();
        createSigninCoordinator();
        ThreadUtils.runOnUiThreadBlocking(() -> mCoordinator.startSigninFlow(config));
    }

    private void createSigninCoordinator() {
        ChromeTabbedActivity baseActivity = mBaseActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            baseActivity.getProfileProviderSupplier().get().getOriginalProfile();
                    OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
                    profileSupplier.set(profile);
                    mCoordinator =
                            BottomSheetSigninAndHistorySyncCoordinator
                                    .createAndObserveAddAccountResult(
                                            baseActivity.getWindowAndroid(),
                                            /* activity= */ baseActivity,
                                            /* activityResultTracker= */ baseActivity
                                                    .getActivityResultTracker(),
                                            /* delegate= */ mDelegate,
                                            DeviceLockActivityLauncherImpl.get(),
                                            profileSupplier,
                                            this::getBottomSheetController,
                                            baseActivity.getModalDialogManagerSupplier(),
                                            baseActivity.getSnackbarManager(),
                                            mSigninAccessPoint);
                });
    }

    private void launchActivity(
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode) {
        // These histograms are recorded in the SigninAndHistorySync activity but they should
        // only be recorded in the fullscreen case.
        HistogramWatcher fullscreenActivityHistograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Signin.Timestamps.Android.Fullscreen.TriggerLayoutInflation")
                        .expectNoRecords("Signin.Timestamps.Android.Fullscreen.ActivityInflated")
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                createConfig(noAccountSigninMode, withAccountSigninMode, historyOptInMode, null);

        Intent intent =
                SigninAndHistorySyncActivity.createIntent(
                        ApplicationProvider.getApplicationContext(), config, mSigninAccessPoint);
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
        fullscreenActivityHistograms.assertExpected();
    }

    private BottomSheetSigninAndHistorySyncConfig createConfig(
            @NoAccountSigninMode int noAccountSigninMode,
            @WithAccountSigninMode int withAccountSigninMode,
            @HistorySyncConfig.OptInMode int historyOptInMode,
            @Nullable CoreAccountId accountId) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder("Title").build();
        BottomSheetSigninAndHistorySyncConfig.Builder builder =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                        bottomSheetStrings,
                        noAccountSigninMode,
                        withAccountSigninMode,
                        historyOptInMode,
                        "Title",
                        "Subtitle");
        if (withAccountSigninMode == WithAccountSigninMode.SEAMLESS_SIGNIN) {
            assert accountId != null;
            builder = builder.useSeamlessWithAccountSignin(accountId);
        }
        return builder.build();
    }

    private void verifyCollapsedBottomSheetAndSignin(CoreAccountInfo accountInfo) {
        HistogramWatcher signinHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Signin.SignIn.Started", mSigninAccessPoint)
                        .expectAnyRecord("Signin.SignIn.Timestamps.Other.ManagementStatusLoaded")
                        .expectAnyRecord("Signin.SignIn.Timestamps.Other.SigninCompleted")
                        .build();

        // Start sign-in from the collapsed sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed)),
                                isCompletelyDisplayed()))
                .perform(click());

        signinHistogramWatcher.assertExpected();

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(accountInfo);
        if (DeviceInfo.isAutomotive()) {
            verify(mDeviceLockActivityLauncher)
                    .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
        }
    }

    private void verifyNoAccountBottomSheetAndSignin() {
        HistogramWatcher signinHistogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Signin.SignIn.Started", mSigninAccessPoint)
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.ACTIVITY_SURVIVED)
                        .expectAnyRecord("Signin.SignIn.Timestamps.Other.ManagementStatusLoaded")
                        .expectAnyRecord("Signin.SignIn.Timestamps.Other.SigninCompleted")
                        .build();

        // Start sign-in from the 0-account sign-in bottom-sheet shown.
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_no_account)),
                                isCompletelyDisplayed()))
                .perform(click());
        mSigninTestRule.setAddAccountFlowResult(TestAccounts.AADC_ADULT_ACCOUNT);
        onViewWaiting(SigninTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        signinHistogramWatcher.assertExpected();

        mSigninTestRule.waitForSignin(TestAccounts.AADC_ADULT_ACCOUNT);
        if (DeviceInfo.isAutomotive()) {
            verify(mDeviceLockActivityLauncher)
                    .launchDeviceLockActivity(any(), any(), anyBoolean(), any(), any(), any());
        }
    }

    private void acceptHistorySyncAndVerifyFlowCompletion(boolean hasSignedIn) {
        // Verify that the history opt-in dialog is shown and accept.
        waitForView(withId(R.id.history_sync_illustration));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        // Verify history sync state.
        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify flow completion.
        if (SigninFeatureMap.isEnabled(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)) {
            verifyHistorySyncDialogDismissed();
            verify(mDelegate, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                    .onFlowComplete(
                            eq(
                                    new Result(
                                            /* hasSignedIn= */ hasSignedIn,
                                            /* hasOptedInHistorySync= */ true)));
        } else {
            ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        }
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

    private void verifyHistorySyncDialogDismissed() {
        onView(withId(R.id.history_sync_illustration)).check(doesNotExist());
    }

    private BottomSheetController getBottomSheetController() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return BottomSheetControllerProvider.from(
                            mBaseActivityTestRule.getActivity().getWindowAndroid());
                });
    }
}
