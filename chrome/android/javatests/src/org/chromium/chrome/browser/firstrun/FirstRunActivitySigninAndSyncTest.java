// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.FREMobileIdentityConsistencyFieldTrial;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Integration tests for the first run experience with sign-in and sync decoupled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
@DoNotBatch(reason = "This test interacts with native initialization")
public class FirstRunActivitySigninAndSyncTest {
    private static final String TEST_EMAIL = "test.account@gmail.com";
    private static final String CHILD_EMAIL = "child.account@gmail.com";
    private static final String TEST_URL = "https://foo.com";

    @Rule
    public final TestRule mCommandLindFlagRule = CommandLineFlags.getTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    // TODO(https://crbug.com/1352119): Use IdentityIntegrationTestRule instead.
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeAccountManagerFacade(), null);

    // TODO(crbug.com/1311260): Consider using a test rule to ensure this gets terminated correctly.
    public FirstRunActivity mFirstRunActivity;

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private LocaleManagerDelegate mLocalManagerDelegateMock;

    @Before
    public void setUp() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.DONT_SHOW);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocaleManager.getInstance().setDelegateForTest(mLocalManagerDelegateMock);
        });
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
    }

    @Test
    @MediumTest
    public void dismissButtonClickSkipsSyncConsentPageWhenNoAccountsAreOnDevice() {
        launchFirstRunActivityAndWaitForNativeInitialization();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_dismiss_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void dismissButtonClickSkipsSyncConsentPageWhenOneAccountIsOnDevice() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSyncConsentPage() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @LargeTest
    public void continueButtonCanBeClickedBeforeNativeIsLoaded() {
        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.INITIALIZATION_FLOW_NEW_GROUP);
        // Suppress native initialization to verify that SigninFirstRunFragment doesn't require it
        // to proceed past the initial loading spinner.
        CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        AccountInfo accountInfo = mAccountManagerTestRule.addAccount(TEST_EMAIL);

        launchFirstRunActivity();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        // Native should still be uninitialized.
        assertFalse(ChromeBrowserInitializer.getInstance().isFullBrowserInitialized());

        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mFirstRunActivity.startDelayedNativeInitializationForTests());

        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                    Profile.getLastUsedRegularProfile());
            assertEquals(CoreAccountInfo.getIdFrom(
                                 identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)),
                    accountInfo.getId());
        });
    }

    @Test
    @LargeTest
    public void dismissButtonCanBeClickedBeforeNativeIsLoaded() {
        FREMobileIdentityConsistencyFieldTrial.setFirstRunTrialGroupForTesting(
                FREMobileIdentityConsistencyFieldTrial.INITIALIZATION_FLOW_NEW_GROUP);
        // Suppress native initialization to verify that SigninFirstRunFragment doesn't require it
        // to proceed past the initial loading spinner.
        CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);

        launchFirstRunActivity();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        // Native should still be uninitialized.
        assertFalse(ChromeBrowserInitializer.getInstance().isFullBrowserInitialized());

        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mFirstRunActivity.startDelayedNativeInitializationForTests());

        // When the native is loaded, First Run should be completed.
        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                    Profile.getLastUsedRegularProfile());
            assertNull(identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        });
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @DisabledTest(message = "https://crbug.com/1382901")
    public void continueButtonClickShowsSyncConsentPageWithChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        // SigninChecker should have been created and have signed the user in.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SIGNIN);
        });

        clickButton(R.id.signin_fre_continue_button);

        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    public void dismissButtonNotShownOnResetForChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView((withId(R.id.signin_fre_dismiss_button))).check(matches(not(isDisplayed())));

        onView((withId(R.id.signin_fre_continue_button))).perform(click());
        Espresso.pressBack();

        onView((withId(R.id.signin_fre_dismiss_button))).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void continueButtonClickSkipsSyncConsentPageWhenCannotUseGooglePlayServices() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_continue_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSearchEnginePageWhenItIsEnabled() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.SHOW_NEW);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        waitUntilCurrentPageIs(DefaultSearchEngineFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void dismissButtonClickShowsSearchEnginePageWhenItIsEnabled() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.SHOW_NEW);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        waitUntilCurrentPageIs(DefaultSearchEngineFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void acceptingSyncEndsFreAndEnablesSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.positive_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForSyncFeatureEnabled();
    }

    @Test
    @MediumTest
    public void refusingSyncEndsFreAndDoesNotEnableSync() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.negative_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        assertFalse(SyncTestUtil.canSyncFeatureStart());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1335094")
    public void clickingSettingsEndsFreAndStartsEnablingSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        onView(withId(R.id.signin_details_description)).perform(new LinkClick());

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        // Sync-the-feature can start but won't become enabled until the user clicks the "Confirm"
        // button in settings.
        SyncTestUtil.waitForCanSyncFeatureStart();
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @DisabledTest(message = "https://crbug.com/1392133")
    public void acceptingSyncForChildAccountEndsFreAndEnablesSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));
        clickButton(R.id.signin_fre_continue_button);
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.positive_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForSyncFeatureEnabled();
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    public void refusingSyncForChildAccountEndsFreAndDoesNotEnableSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));
        clickButton(R.id.signin_fre_continue_button);
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.negative_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        assertFalse(SyncTestUtil.canSyncFeatureStart());
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    public void clickingSettingsThenCancelForChildAccountDoesNotEnableSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        onView(withId(R.id.signin_details_description)).perform(new LinkClick());

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        // Sync-the-feature can start but won't become enabled until the user clicks the "Confirm"
        // button in settings.
        SyncTestUtil.waitForCanSyncFeatureStart();

        // Check that the sync consent has been cleared (but the user is still signed in), and that
        // the sync service state changes have been undone.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        });

        // Click the cancel button to exit the activity.
        onView(withId(R.id.cancel_button)).perform(click());

        // Check that the sync consent has been cleared (but the user is still signed in), and that
        // the sync service state changes have been undone.
        CriteriaHelper.pollUiThread(() -> {
            IdentityManager identityManager =
                    IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .getIdentityManager();
            return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                    && !identityManager.hasPrimaryAccount(ConsentLevel.SYNC);
        });
    }

    private void clickButton(@IdRes int buttonId) {
        // Ensure that the button isn't hidden, and is enabled.
        onView(allOf(withId(buttonId), withEffectiveVisibility(Visibility.VISIBLE)))
                .check(matches(isEnabled()));
        // This helps to reduce flakiness on some marshmallow bots in comparison with
        // espresso click.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mFirstRunActivity.findViewById(buttonId).performClick(); });
    }

    private <T extends FirstRunFragment> void waitUntilCurrentPageIs(Class<T> fragmentClass) {
        CriteriaHelper.pollUiThread(() -> {
            return fragmentClass.isInstance(mFirstRunActivity.getCurrentFragmentForTesting());
        }, fragmentClass.getName() + " should be the current page");
    }

    private void launchFirstRunActivityAndWaitForNativeInitialization() {
        launchFirstRunActivity();
        CriteriaHelper.pollUiThread(
                () -> mFirstRunActivity.getNativeInitializationPromise().isFulfilled());
    }

    private void launchFirstRunActivity() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        // The FirstRunActivity relaunches the original intent when it finishes, see
        // FirstRunActivityBase.EXTRA_FRE_COMPLETE_LAUNCH_INTENT. So to guarantee that
        // ChromeTabbedActivity gets started, we must ask for more than just FirstRunActivity
        // here.
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL));
        intent.setPackage(context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mFirstRunActivity = ApplicationTestUtils.waitForActivityWithClass(
                FirstRunActivity.class, Stage.RESUMED, () -> context.startActivity(intent));
    }

    private static class LinkClick implements ViewAction {
        @Override
        public Matcher<View> getConstraints() {
            return Matchers.instanceOf(TextView.class);
        }

        @Override
        public String getDescription() {
            return "Clicks on the one and only clickable link in the view.";
        }

        @Override
        public void perform(UiController uiController, View view) {
            final TextView textView = (TextView) view;
            final Spanned spannedString = (Spanned) textView.getText();
            final ClickableSpan[] spans =
                    spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
            assertEquals("There should be only one clickable link.", 1, spans.length);
            spans[0].onClick(view);
        }
    };
}
