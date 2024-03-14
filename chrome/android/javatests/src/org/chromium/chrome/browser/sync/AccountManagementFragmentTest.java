// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;

/** Tests {@link AccountManagementFragment}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "TODO(crbug.com/1168590): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountManagementFragmentTest {
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail("account@gmail.com");

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<AccountManagementFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccountManagementFragment.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work (SyncTestRule extends CTARule).
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    public static class MigrateAccountManagementSettingsToCapabilitiesParams
            implements ParameterProvider {

        private static List<ParameterSet> sMigrateAccountManagementSettingsToCapabilities =
                Arrays.asList(
                        new ParameterSet()
                                .value(true)
                                .name("MigrateProfileIsChildFlagParamsEnabled"),
                        new ParameterSet()
                                .value(false)
                                .name("MigrateProfileIsChildFlagParamsDisabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sMigrateAccountManagementSettingsToCapabilities;
        }
    }

    @ParameterAnnotations.UseMethodParameterBefore(
            MigrateAccountManagementSettingsToCapabilitiesParams.class)
    public void enableFlag(boolean isMigrateAccountManagementSettingsToCapabilitiesFlagEnabled) {
        FeatureList.TestValues testValuesOverride = new FeatureList.TestValues();
        testValuesOverride.addFeatureFlagOverride(
                ChromeFeatureList.MIGRATE_ACCOUNT_MANAGEMENT_SETTINGS_TO_CAPABILITIES,
                isMigrateAccountManagementSettingsToCapabilitiesFlagEnabled);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAccountManagementFragmentView() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_view");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testSignedInAccountShownOnTop() throws Exception {
        mSyncTestRule.addAccount("testSecondary@gmail.com");
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSettingsActivityTestRule.startSettingsActivity();
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(view, "account_management_fragment_signed_in_account_on_top");
    }

    @Test
    @MediumTest
    public void testAccountManagementViewForChildAccountWithNonDisplayableAccountEmail()
            throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        AccountInfo accountInfo =
                signinTestRule.addAccount(
                        CHILD_ACCOUNT_NAME,
                        SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        signinTestRule.waitForSeeding();
        signinTestRule.waitForSignin(accountInfo);
        mSettingsActivityTestRule.startSettingsActivity();

        // Force update the fragment so that NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES is
        // actually utilized. This is to replicate downstream implementation behavior, where
        // checkIfDisplayableEmailAddress() differs.
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .getProfileDataOrDefault(CHILD_ACCOUNT_NAME)
                            .hasDisplayableEmailAddress();
                });
        TestThreadUtils.runOnUiThreadBlocking(mSettingsActivityTestRule.getFragment()::update);
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        onView(
                        allOf(
                                isDescendantOfA(withId(android.R.id.list_container)),
                                withText(accountInfo.getFullName())))
                .check(matches(isDisplayed()));
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void
            testAccountManagementViewForChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName()
                    throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo accountInfo =
                signinTestRule.addAccount(
                        CHILD_ACCOUNT_NAME,
                        "",
                        "",
                        null,
                        SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        signinTestRule.waitForSeeding();
        signinTestRule.waitForSignin(accountInfo);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .getProfileDataOrDefault(CHILD_ACCOUNT_NAME)
                            .hasDisplayableEmailAddress();
                });
        TestThreadUtils.runOnUiThreadBlocking(mSettingsActivityTestRule.getFragment()::update);
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
        onView(
                        allOf(
                                isDescendantOfA(withId(android.R.id.list_container)),
                                withText(R.string.default_google_account_username)))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            MigrateAccountManagementSettingsToCapabilitiesParams.class)
    public void testAccountManagementViewForChildAccount(
            boolean isMigrateAccountManagementSettingsToCapabilitiesFlagEnabled) throws Exception {
        final AccountCapabilitiesBuilder accountCapabilitiesBuilder =
                new AccountCapabilitiesBuilder();
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo primarySupervisedAccount =
                signinTestRule.addAccount(
                        CHILD_ACCOUNT_NAME,
                        accountCapabilitiesBuilder.setIsSubjectToParentalControls(true).build());
        signinTestRule.waitForSeeding();
        signinTestRule.waitForSignin(primarySupervisedAccount);

        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .hasProfileDataForTesting(CHILD_ACCOUNT_NAME);
                });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view,
                "account_management_fragment_for_child_account_with_add_account_for_supervised_users");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(
            MigrateAccountManagementSettingsToCapabilitiesParams.class)
    public void testAccountManagementViewForChildAccountWithSecondaryEduAccount(
            boolean isMigrateAccountManagementSettingsToCapabilitiesFlagEnabled) throws Exception {
        final AccountCapabilitiesBuilder accountCapabilitiesBuilder =
                new AccountCapabilitiesBuilder();
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo primarySupervisedAccount =
                signinTestRule.addAccount(
                        CHILD_ACCOUNT_NAME,
                        accountCapabilitiesBuilder.setIsSubjectToParentalControls(true).build());
        signinTestRule.addAccount("account@school.com");
        signinTestRule.waitForSeeding();
        signinTestRule.waitForSignin(primarySupervisedAccount);

        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .hasProfileDataForTesting(CHILD_ACCOUNT_NAME);
                });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view,
                "account_management_fragment_for_child_and_edu_accounts_with_add_account_for_supervised_users");
    }

    @Test
    @SmallTest
    public void testSignOutUserWithoutShowingSignOutDialog() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Account should be signed out!",
                                IdentityServicesProvider.get()
                                        .getIdentityManager(
                                                ProfileManager.getLastUsedRegularProfile())
                                        .hasPrimaryAccount(ConsentLevel.SIGNIN)));
    }

    @Test
    @SmallTest
    public void showSignOutDialogBeforeSigningUserOut() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.turn_off_sync_and_signout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SYNC_SHOW_IDENTITY_ERRORS_FOR_SIGNED_IN_USERS)
    public void testIdentityErrorCardShownForSignedInUsers() {
        // Fake an identity error.
        overrideSyncService().setRequiresClientUpgrade(true);

        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.identity_error_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SYNC_SHOW_IDENTITY_ERRORS_FOR_SIGNED_IN_USERS)
    public void testIdentityErrorCardNotShownIfFeatureDisabled() {
        // Fake an identity error.
        overrideSyncService().setRequiresClientUpgrade(true);

        // Expect no records.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.identity_error_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SYNC_SHOW_IDENTITY_ERRORS_FOR_SIGNED_IN_USERS)
    public void testIdentityErrorCardNotShownIfNoError() {
        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.identity_error_card)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SYNC_SHOW_IDENTITY_ERRORS_FOR_SIGNED_IN_USERS)
    public void testIdentityErrorCardNotShownForSyncingUsers() {
        // Fake an identity error.
        overrideSyncService().setRequiresClientUpgrade(true);

        // Expect no records.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in, enable sync and open settings.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.identity_error_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SYNC_SHOW_IDENTITY_ERRORS_FOR_SIGNED_IN_USERS)
    public void testIdentityErrorCardDynamicallyShownOnError() {
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();

        // Expect no records initially.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        // No error card exists right now.
        onView(withId(R.id.identity_error_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();

        watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);

        // Error card is showing now.
        onViewWaiting(withId(R.id.identity_error_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SYNC_SHOW_IDENTITY_ERRORS_FOR_SIGNED_IN_USERS)
    public void testIdentityErrorCardDynamicallyHidden() {
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);

        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        // The error card exists right now.
        onView(withId(R.id.identity_error_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();

        // Expect no records now.
        watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Clear the error.
        fakeSyncService.setRequiresClientUpgrade(false);

        // No error card exists anymore.
        onView(withId(R.id.identity_error_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    private FakeSyncServiceImpl overrideSyncService() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    FakeSyncServiceImpl fakeSyncService = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncService);
                    return fakeSyncService;
                });
    }
}
