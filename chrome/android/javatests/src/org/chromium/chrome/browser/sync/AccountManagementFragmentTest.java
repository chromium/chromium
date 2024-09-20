// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.core.StringStartsWith.startsWith;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.view.View;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;

import java.util.Set;

/** Tests {@link AccountManagementFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountManagementFragmentTest {
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

    @Rule public final JniMocker mJniMocker = new JniMocker();

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        // Prevent "GmsCore outdated" error from being exposed in bots with old version.
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(false);
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
                signinTestRule.addChildTestAccountThenWaitForSignin(
                        new AccountCapabilitiesBuilder().setCanHaveEmailAddressDisplayed(false));
        mSettingsActivityTestRule.startSettingsActivity();

        // Force update the fragment so that NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES is
        // actually utilized. This is to replicate downstream implementation behavior, where
        // checkIfDisplayableEmailAddress() differs.
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .getProfileDataOrDefault(accountInfo.getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(mSettingsActivityTestRule.getFragment()::update);
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
        AccountInfo accountInfo =
                AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME;
        signinTestRule.addAccountThenSignin(accountInfo);
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .getProfileDataOrDefault(accountInfo.getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(mSettingsActivityTestRule.getFragment()::update);
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
    public void testAccountManagementViewForChildAccount() throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo primarySupervisedAccount =
                signinTestRule.addChildTestAccountThenWaitForSignin();

        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .hasProfileDataForTesting(primarySupervisedAccount.getEmail());
                });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view,
                "account_management_fragment_for_child_account_with_add_account_for_supervised_"
                        + "users");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAccountManagementViewForChildAccountWithSecondaryEduAccount() throws Exception {
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        CoreAccountInfo primarySupervisedAccount =
                signinTestRule.addChildTestAccountThenWaitForSignin();
        signinTestRule.addAccount("account@school.com");
        signinTestRule.waitForSignin(primarySupervisedAccount);

        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                            .getFragment()
                            .getProfileDataCacheForTesting()
                            .hasProfileDataForTesting(primarySupervisedAccount.getEmail());
                });
        View view = mSettingsActivityTestRule.getFragment().getView();
        onViewWaiting(allOf(is(view), isDisplayed()));
        mRenderTestRule.render(
                view,
                "account_management_fragment_for_child_and_edu_accounts_with_add_account_for_"
                        + "supervised_users");
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignOutUserWithoutShowingSignOutDialog() {
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        fakeSyncService.setTypesWithUnsyncedData(Set.of(DataType.BOOKMARKS));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out)).perform(click());
        ThreadUtils.runOnUiThreadBlocking(
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
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testSignOutShowsUnsavedDataDialog() {
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        fakeSyncService.setTypesWithUnsyncedData(Set.of(DataType.BOOKMARKS));

        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.sign_out)).perform(click());

        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
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
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardNotShownIfNoError() {
        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();

        watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN);

        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);

        // Error card is showing now.
        onViewWaiting(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));
        watchIdentityErrorCardShownHistogram.assertExpected();

        // Expect no records now.
        watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Clear the error.
        fakeSyncService.setRequiresClientUpgrade(false);

        // No error card exists anymore.
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testIdentityErrorCardNotShownIfFlagEnabled() {
        // Fake an identity error.
        overrideSyncService().setRequiresClientUpgrade(true);

        // Expect no records.
        HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Sync.IdentityErrorCard.ClientOutOfDate")
                        .build();

        // Sign in, enable sync and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSettingsActivityTestRule.startSettingsActivity();

        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
        watchIdentityErrorCardShownHistogram.assertExpected();
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testActionForAuthError() throws Exception {
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        fakeSyncService.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();

        mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        // The error card exists.
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));

        FakeAccountManagerFacade fakeAccountManagerFacade =
                spy((FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance());
        AccountManagerFacadeProvider.setInstanceForTests(fakeAccountManagerFacade);

        doAnswer(
                        invocation -> {
                            // Simulate re-auth by clearing the auth error.
                            fakeSyncService.setAuthError(GoogleServiceAuthError.State.NONE);
                            return null;
                        })
                .when(fakeAccountManagerFacade)
                .updateCredentials(any(), any(), any());

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        // No error card exists anymore.
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testActionForPassphraseRequired() throws Exception {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");

        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();

        SyncService syncService = mSyncTestRule.getSyncService();
        CriteriaHelper.pollUiThread(() -> syncService.isPassphraseRequiredForPreferredDataTypes());

        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        // The error card exists.
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        final AccountManagementFragment fragment = mSettingsActivityTestRule.getFragment();
        // Passphrase dialog should open.
        final PassphraseDialogFragment passphraseFragment =
                ActivityTestUtils.waitForFragment(
                        settingsActivity, AccountManagementFragment.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting the passphrase
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        ThreadUtils.runOnUiThreadBlocking(() -> syncService.setDecryptionPassphrase("passphrase"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    fragment.getFragmentManager().executePendingTransactions();
                    Assert.assertNull(
                            "PassphraseDialogFragment should be dismissed.",
                            settingsActivity
                                    .getFragmentManager()
                                    .findFragmentByTag(
                                            AccountManagementFragment.FRAGMENT_ENTER_PASSPHRASE));
                });

        // No error card exists anymore.
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testActionForClientOutdatedError() throws Exception {
        overrideSyncService().setRequiresClientUpgrade(true);

        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();

        mSettingsActivityTestRule.startSettingsActivity();
        onViewWaiting(allOf(is(mSettingsActivityTestRule.getFragment().getView()), isDisplayed()));
        // The error card exists.
        onView(withId(R.id.signin_settings_card)).check(matches(isDisplayed()));

        Intents.init();
        // Stub all external intents.
        intending(IntentMatchers.anyIntent())
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        // Mimic the user tapping on the error card's button.
        onView(withId(R.id.signin_settings_card_button)).perform(click());

        intended(IntentMatchers.hasDataString(startsWith("market")));
        Intents.release();
    }

    private FakeSyncServiceImpl overrideSyncService() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeSyncServiceImpl fakeSyncService = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncService);
                    return fakeSyncService;
                });
    }
}
