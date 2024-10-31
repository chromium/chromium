// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.widget.ProgressBar;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.SigninUtils;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.ViewUtils;

/** Integration tests for the re-FRE. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// The upgrade promo does not get displayed when Google Play Services are not available or on
// Android Automotive.
@Restriction({
    DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_20W02
})
public class FullscreenSigninAndHistorySyncIntegrationTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 1)
    public final BaseActivityTestRule<BlankUiTestActivity> mBlankUiActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Rule(order = 2)
    public final BaseActivityTestRule<SigninAndHistorySyncActivity> mActivityTestRule =
            new BaseActivityTestRule(SigninAndHistorySyncActivity.class);

    @Mock private HistorySyncHelper mHistorySyncHelperMock;

    private SigninAndHistorySyncActivity mActivity;
    private @SigninAccessPoint int mSigninAccessPoint = SigninAccessPoint.SIGNIN_PROMO;
    private @HistorySyncConfig.OptInMode int mHistoryOptInMode =
            HistorySyncConfig.OptInMode.OPTIONAL;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_refuseSignin() {
        HistogramWatcher accountConsistencyHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AccountConsistencyPromoAction",
                                AccountConsistencyPromoAction.SHOWN)
                        .expectIntRecord(
                                "Signin.AccountConsistencyPromoAction.Shown", mSigninAccessPoint)
                        .build();
        HistogramWatcher accountStartedHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Started", mSigninAccessPoint);

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        // Check that the privacy disclaimer is not displayed.
        onView(withId(R.id.signin_fre_footer)).check(matches(not(isDisplayed())));
        // Refuse sign-in.
        onViewWaiting(withId(R.id.signin_fre_dismiss_button)).perform(click());

        accountConsistencyHistogram.assertExpected();
        accountStartedHistogram.assertExpected();
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_refuseHistorySync_historySyncOptional() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown and refuse.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock, never()).recordHistorySyncNotShown(anyInt());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_refuseHistorySync_historySyncRequired() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        mHistoryOptInMode = HistorySyncConfig.OptInMode.REQUIRED;

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown and refuse.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        onViewWaiting(withId(org.chromium.chrome.test.R.id.button_secondary)).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock, never()).recordHistorySyncNotShown(anyInt());

        // Verify that the user is signed-out.
        assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_acceptHistorySync_historySyncOptional() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        verify(mHistorySyncHelperMock, never()).recordHistorySyncNotShown(anyInt());
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_acceptHistorySync_historySyncRequired() {
        mHistoryOptInMode = HistorySyncConfig.OptInMode.REQUIRED;

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        verify(mHistorySyncHelperMock, never()).recordHistorySyncNotShown(anyInt());
        verify(mHistorySyncHelperMock, never()).isDeclinedOften();
    }

    @Test
    @MediumTest
    public void testWithExistingAccount_signIn_noHistorySync() {
        mHistoryOptInMode = HistorySyncConfig.OptInMode.NONE;

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_continue_button)).perform(click());

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        mSigninTestRule.waitForSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        Assert.assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
    }

    @Test
    @MediumTest
    public void testHistorySyncSuppressed_historySyncOptional() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.signin_fre_continue_button)).perform(click());

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        mSigninTestRule.waitForSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        Assert.assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
    }

    @Test
    @MediumTest
    public void testHistorySyncSuppressed_historySyncRequired() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);
        mHistoryOptInMode = HistorySyncConfig.OptInMode.REQUIRED;
        mSigninAccessPoint = SigninAccessPoint.RECENT_TABS;

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.signin_fre_continue_button)).perform(click());

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        mSigninTestRule.waitForSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        Assert.assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
    }

    @Test
    @MediumTest
    public void testHistorySyncDeclinedOften_historySyncOptional() {
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.signin_fre_continue_button)).perform(click());

        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        mSigninTestRule.waitForSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        Assert.assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock).recordHistorySyncNotShown(mSigninAccessPoint);
    }

    @Test
    @MediumTest
    public void testUserAlreadySignedIn_onlyShowsHistorySync() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);

        launchActivity(/* shouldReplaceProgressBars= */ false);

        // Verify that the history opt-in dialog is shown and accept.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testUserAlreadySignedIn_refuseHistorySync_historySyncRequired() {
        mSigninTestRule.addAccountThenSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        mHistoryOptInMode = HistorySyncConfig.OptInMode.REQUIRED;

        launchActivity(/* shouldReplaceProgressBars= */ false);

        // Verify that the history opt-in dialog is shown and refuse.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        onViewWaiting(withId(org.chromium.chrome.test.R.id.button_secondary)).perform(click());

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        verify(mHistorySyncHelperMock, never()).recordHistorySyncNotShown(anyInt());

        // Verify that the user is not signed-out.
        Assert.assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    // There is an issue causing {@link Activity.setRequestedOrientation} to throw an exception in
    // Android 8 which was fixed in Android 8.1. See b/70718000 for example.
    @MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
    public void testScreenRotation() {
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);

        launchActivity();

        // Rotate the screen.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivity, Configuration.ORIENTATION_LANDSCAPE);

        // Verify that the view switcher is displayed with the correct layout.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        if (SigninUtils.shouldShowDualPanesHorizontalLayout(mActivity)) {
            onViewWaiting(withId(R.id.fullscreen_signin_and_history_sync_landscape))
                    .check(matches(isDisplayed()));
        } else {
            onView(withId(R.id.fullscreen_signin_and_history_sync_portrait))
                    .check(matches(isDisplayed()));
        }

        // Sign in.
        onView(withId(R.id.signin_fre_continue_button)).perform(scrollTo()).perform(click());

        // Verify that the view is displayed with the correct layout.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        if (SigninUtils.shouldShowDualPanesHorizontalLayout(mActivity)) {
            onView(withId(R.id.fullscreen_signin_and_history_sync_landscape))
                    .check(matches(isDisplayed()));
        } else {
            onView(withId(R.id.fullscreen_signin_and_history_sync_portrait))
                    .check(matches(isDisplayed()));
        }

        // Rotate the screen back.
        ActivityTestUtils.rotateActivityToOrientation(
                mActivity, Configuration.ORIENTATION_PORTRAIT);
        onView(withId(R.id.fullscreen_signin_and_history_sync_portrait))
                .check(matches(isDisplayed()));

        // Accept history sync.
        onView(allOf(withId(R.id.button_primary), isCompletelyDisplayed())).perform(click());

        SyncTestUtil.waitForHistorySyncEnabled();

        // Verify that the flow completion callback, which finishes the activity, is called.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        launchActivity();

        // Verify that the fullscreen sign-in promo is shown with the default account.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onViewWaiting(withText(AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .check(matches(isDisplayed()));

        // Add the second account.
        onView(withText(AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail())).perform(click());
        onView(withText(R.string.signin_add_account_to_device)).perform(click());
        mSigninTestRule.setAddAccountFlowResult(TestAccounts.ACCOUNT2.getEmail());
        onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER).perform(click());

        // Verify that the fullscreen sign-in promo is shown with the newly added account.
        onViewWaiting(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onViewWaiting(withText(TestAccounts.ACCOUNT2.getEmail())).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testBackPress() {
        mBlankUiActivityTestRule.launchActivity(null);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);

        launchActivity();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_continue_button)).perform(click());
        mSigninTestRule.waitForSignin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        // Verify that the history opt-in dialog is shown press back.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        Espresso.pressBack();

        // Verify that the fullscreen sign-in promo is shown and press back again.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        Espresso.pressBack();

        // Verify that the flow completion callback, which finishes the activity, is called and that
        // the user was signed out.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testUserAlreadySignedIn_backpress_historySyncOptional() {
        mBlankUiActivityTestRule.launchActivity(null);
        mSigninTestRule.addTestAccountThenSignin();
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);

        launchActivity(/* shouldReplaceProgressBars= */ false);

        // Verify that the history opt-in dialog is shown and press back.
        onView(withId(R.id.history_sync)).check(matches(isDisplayed()));
        Espresso.pressBack();

        // Verify that the flow completion callback, which finishes the activity, is called and that
        // history sync was not enabled.
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.DESTROYED);
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
        Assert.assertNotNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @LargeTest
    @Policies.Add({@Policies.Item(key = "RandomPolicy", string = "true")})
    public void testFragmentWhenSigninIsDisabledByPolicy() {
        launchActivity();

        // Verify that the fullscreen sign-in promo is shown.
        onView(withId(R.id.fullscreen_signin)).check(matches(isDisplayed()));
        // Management notice shouldn't be shown in the upgrade promo.
        onView(withId(R.id.fre_browser_managed_by)).check(matches(not(isDisplayed())));
    }

    @Test
    @LargeTest
    public void testFullscreenSigninCustomisation() {
        // Create a config which only uses non-default resource values to test customisation.
        // For instance, the default sign-in strings are used for history sync and vice versa.
        FullscreenSigninAndHistorySyncConfig config =
                new FullscreenSigninAndHistorySyncConfig.Builder()
                        .signinTitleId(R.string.history_sync_title)
                        .signinSubtitleId(R.string.history_sync_subtitle)
                        .signinLogoId(R.drawable.ic_globe_24dp)
                        .signinDismissTextId(R.string.signin_add_account_to_device)
                        .historySyncTitleId(R.string.signin_fre_title)
                        .historySyncSubtitleId(R.string.signin_fre_subtitle)
                        .build();
        launchActivity(/* shouldReplaceProgressBars= */ true, config);

        // Verify that the strings are corrects and that the logo is shown.
        onView(allOf(withId(R.id.title), withText(R.string.history_sync_title)))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.subtitle), withText(R.string.history_sync_subtitle)))
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.signin_fre_dismiss_button),
                                withText(R.string.signin_add_account_to_device)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.fre_logo)).check(matches(isDisplayed()));

        onView(withId(R.id.signin_fre_continue_button)).perform(click());

        // Verify that the history opt-in dialog is shown with custom strings.
        onView(allOf(withId(R.id.history_sync_title), withText(R.string.signin_fre_title)))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.history_sync_subtitle), withText(R.string.signin_fre_subtitle)))
                .check(matches(isDisplayed()));
    }

    private void launchActivity() {
        launchActivity(true);
    }

    private void launchActivity(boolean shouldReplaceProgressBars) {
        FullscreenSigninAndHistorySyncConfig config =
                new FullscreenSigninAndHistorySyncConfig.Builder()
                        .historyOptInMode(mHistoryOptInMode)
                        .build();
        launchActivity(shouldReplaceProgressBars, config);
    }

    private void launchActivity(
            boolean shouldReplaceProgressBars, FullscreenSigninAndHistorySyncConfig config) {
        Intent intent =
                SigninAndHistorySyncActivity.createIntentForFullscreenSignin(
                        ApplicationProvider.getApplicationContext(), config, mSigninAccessPoint);
        mActivityTestRule.launchActivity(intent);
        mActivity = mActivityTestRule.getActivity();
        ApplicationTestUtils.waitForActivityState(mActivity, Stage.RESUMED);

        if (shouldReplaceProgressBars) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // Replace all the progress bars with placeholders. Currently the progress
                        // bar cannot be stopped otherwise due to some espresso issues (see
                        // crbug.com/40144184).
                        ProgressBar nativeAndPolicyProgressSpinner =
                                mActivity.findViewById(
                                        R.id.fre_native_and_policy_load_progress_spinner);
                        nativeAndPolicyProgressSpinner.setIndeterminateDrawable(
                                new ColorDrawable(SemanticColorUtils.getDefaultBgColor(mActivity)));
                        ProgressBar signinProgressSpinner =
                                mActivity.findViewById(R.id.fre_signin_progress_spinner);
                        signinProgressSpinner.setIndeterminateDrawable(
                                new ColorDrawable(SemanticColorUtils.getDefaultBgColor(mActivity)));
                    });

            ViewUtils.waitForVisibleView(allOf(withId(R.id.fre_logo), isDisplayed()));
        }
    }
}
