// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.concurrent.ExecutionException;

/** Integration tests for the first run experience with sign-in and sync decoupled. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test interacts with native initialization")
public class FirstRunActivitySigninAndSyncTest {
    private static final String TEST_URL = "https://foo.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    // TODO(crbug.com/40234741): Use IdentityIntegrationTestRule instead.
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeAccountManagerFacade(), null);

    // TODO(crbug.com/40830950): Consider using a test rule to ensure this gets terminated
    // correctly.
    public FirstRunActivity mFirstRunActivity;

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock private LocaleManagerDelegate mLocalManagerDelegateMock;

    @Before
    public void setUp() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.DONT_SHOW);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocaleManager.getInstance().setDelegateForTest(mLocalManagerDelegateMock);
                });
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        FirstRunActivity.disableAnimationForTesting(true);
    }

    @After
    public void tearDown() {
        FirstRunActivity.disableAnimationForTesting(false);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void dismissButtonClickSkipsSyncConsentPageWhenNoAccountsAreOnDevice() {
        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Signin.SigninStartedAccessPoint")
                        .build();

        launchFirstRunActivityAndWaitForNativeInitialization();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_dismiss_button);

        signinStartedWatcher.assertExpected();
        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void dismissButtonClickSkipsSyncConsentPageWhenOneAccountIsOnDevice() {
        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Signin.SigninStartedAccessPoint")
                        .build();
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchFirstRunActivityAndWaitForNativeInitialization();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        signinStartedWatcher.assertExpected();
        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void continueButtonClickShowsSyncConsentPage() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        HistogramWatcher signinStartedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);

        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        signinStartedWatcher.assertExpected();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void continueButtonClickShowsHistorySyncPage() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        waitUntilCurrentPageIs(HistorySyncFirstRunFragment.class);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @Features.EnableFeatures({
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS,
    })
    public void destroyHistorySyncActivityWhenAccountIsRemoved() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);

        clickButton(R.id.signin_fre_continue_button);

        // History sync opt-in screen should be displayed.
        waitUntilCurrentPageIs(HistorySyncFirstRunFragment.class);

        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_1.getId());

        // History sync opt-in screen should be dismissed when the primary account is cleared
        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @DisabledTest(message = "https://crbug.com/1382901")
    public void continueButtonClickShowsSyncConsentPageWithChildAccount() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        // SigninChecker should have been created and have signed the user in.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
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
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void continueButtonClickShowsHistorySyncPageWithChildAccount() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        waitUntilCurrentPageIs(HistorySyncFirstRunFragment.class);
        CriteriaHelper.pollUiThread(
                () ->
                        IdentityServicesProvider.get()
                                .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                                .getIdentityManager()
                                .hasPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void dismissButtonNotShownOnResetForChildAccount() throws ExecutionException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));

        onView(withId(R.id.signin_fre_continue_button)).perform(scrollTo(), click());
        completeAutoDeviceLockIfNeeded();
        pressBack();

        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
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
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            continueButtonClickSkipsSyncConsentPageWhenCannotUseGooglePlayServices_historySyncEnabled() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        HistogramWatcher historySyncHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.HistorySyncOptIn.Skipped",
                        org.chromium.components.signin.metrics.SigninAccessPoint.START_PAGE);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_continue_button);

        historySyncHistogram.assertExpected();
        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSearchEnginePageWhenItIsEnabled() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.SHOW_NEW);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        if (!isAutomotive()) {
            onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));
        }

        clickButton(R.id.signin_fre_continue_button);

        completeAutoDeviceLockIfNeeded();

        waitUntilCurrentPageIs(DefaultSearchEngineFirstRunFragment.class);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void dismissButtonClickShowsSearchEnginePageWhenItIsEnabled() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.SHOW_NEW);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        waitUntilCurrentPageIs(DefaultSearchEngineFirstRunFragment.class);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void acceptingSyncEndsFreAndEnablesSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.button_primary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForSyncFeatureEnabled();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void acceptingHistorySyncEndsFreAndEnablesHistorySync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(HistorySyncFirstRunFragment.class);

        onViewWaiting(withId(R.id.button_primary));
        clickButton(R.id.button_primary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForHistorySyncEnabled();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void refusingSyncEndsFreAndDoesNotEnableSync() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickMoreThenClickButton(R.id.button_secondary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        assertFalse(SyncTestUtil.hasSyncConsent());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void refusingHistorySyncEndsFreAndDoesNotEnableHistorySync() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(HistorySyncFirstRunFragment.class);

        onViewWaiting(withId(R.id.button_secondary));
        clickButton(R.id.button_secondary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void clickingSettingsEndsFreAndStartsEnablingSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        onView(withId(R.id.signin_details_description)).perform(new LinkClick());

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        // Sync consent is granted but sync-the-feature won't become enabled until the user clicks
        // the "Confirm" button in settings.
        SyncTestUtil.waitForSyncConsent();
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @DisabledTest(message = "https://crbug.com/1392133")
    public void acceptingSyncForChildAccountEndsFreAndEnablesSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.button_primary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForSyncFeatureEnabled();
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @Features.DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void refusingSyncForChildAccountEndsFreAndDoesNotEnableSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickMoreThenClickButton(R.id.button_secondary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        assertFalse(SyncTestUtil.hasSyncConsent());
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void refusingHistorySyncForChildAccountEndsFreAndDoesNotEnableHistorySync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(HistorySyncFirstRunFragment.class);

        clickButton(R.id.button_secondary);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        assertFalse(SyncTestUtil.isHistorySyncEnabled());
    }

    @Test
    @MediumTest
    // ChildAccountStatusSupplier uses AppRestrictions to quickly detect non-supervised cases,
    // adding at least one policy via AppRestrictions prevents that.
    @Policies.Add(@Policies.Item(key = "ForceSafeSearch", string = "true"))
    @DisabledTest(message = "https://crbug.com/1459076")
    public void clickingSettingsThenCancelForChildAccountDoesNotEnableSync() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        launchFirstRunActivityAndWaitForNativeInitialization();
        waitUntilCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        completeAutoDeviceLockIfNeeded();
        waitUntilCurrentPageIs(SyncConsentFirstRunFragment.class);

        onView(withId(R.id.signin_details_description)).perform(new LinkClick());

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        // Sync consent is granted but sync-the-feature won't become enabled until the user clicks
        // the "Confirm" button in settings.
        SyncTestUtil.waitForSyncConsent();

        // Check that the sync consent has been cleared (but the user is still signed in), and that
        // the sync service state changes have been undone.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });

        // Click the cancel button to exit the activity.
        onView(withId(R.id.cancel_button)).perform(click());

        // Check that the sync consent has been cleared (but the user is still signed in), and that
        // the sync service state changes have been undone.
        CriteriaHelper.pollUiThread(
                () -> {
                    IdentityManager identityManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                                    .getIdentityManager();
                    return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                            && !identityManager.hasPrimaryAccount(ConsentLevel.SYNC);
                });
    }

    private void clickMoreThenClickButton(@IdRes int buttonId) {
        // The more button is shown on smaller screens. Click it if it's visible so the
        // main button bar is shown.
        if (mFirstRunActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }
        clickButton(buttonId);
    }

    private void clickButton(@IdRes int buttonId) {
        // Ensure that the button isn't hidden, and is enabled.
        onView(allOf(withId(buttonId), withEffectiveVisibility(Visibility.VISIBLE)))
                .check(matches(isEnabled()));
        // This helps to reduce flakiness on some marshmallow bots in comparison with
        // espresso click.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFirstRunActivity.findViewById(buttonId).performClick();
                });
    }

    private <T extends FirstRunFragment> void waitUntilCurrentPageIs(Class<T> fragmentClass) {
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                mFirstRunActivity.getCurrentFragmentForTesting(),
                                Matchers.instanceOf(fragmentClass)));
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
        mFirstRunActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        FirstRunActivity.class, Stage.RESUMED, () -> context.startActivity(intent));
    }

    private void completeAutoDeviceLockIfNeeded() {
        if (mFirstRunActivity.getCurrentFragmentForTesting() instanceof SigninFirstRunFragment) {
            SigninTestUtil.completeAutoDeviceLockIfNeeded(
                    (SigninFirstRunFragment) mFirstRunActivity.getCurrentFragmentForTesting());
        }
    }

    private boolean isAutomotive() {
        return BuildInfo.getInstance().isAutomotive;
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
    }
}
