// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;

import java.util.concurrent.TimeoutException;

/** Test suite for SyncErrorCardPreference */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncErrorCardPreferenceTest {
    private final SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(8)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    private FakeSyncServiceImpl mFakeSyncServiceImpl;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);

        // Start main activity before because native side needs to be initialized before overriding
        // SyncService.
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(mFakeSyncServiceImpl);
                });
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    private void assertSyncError(@SyncSettingsUtils.SyncError int expectedSyncError) {
        @SyncSettingsUtils.SyncError
        int currentSyncError =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                SyncSettingsUtils.getSyncError(
                                        ProfileManager.getLastUsedRegularProfile()));
        Assert.assertEquals(
                "SyncError did not match expected value", expectedSyncError, currentSyncError);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForAuthErrorWithUpmEnabled(boolean nightModeEnabled)
            throws Exception {
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.AUTH_ERROR);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.AuthError", SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_auth_error_with_new_title_and_upm");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForClientOutOfDate(boolean nightModeEnabled) throws Exception {
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.CLIENT_OUT_OF_DATE);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_client_out_of_date_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForOtherErrors(boolean nightModeEnabled) throws Exception {
        mFakeSyncServiceImpl.setAuthError(GoogleServiceAuthError.State.CONNECTION_FAILED);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.OTHER_ERRORS);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.OtherErrors", SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_other_errors_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForPassphraseRequired(boolean nightModeEnabled) throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.PASSPHRASE_REQUIRED);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.PassphraseRequired",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_passphrase_required_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForTrustedVaultKey(boolean nightModeEnabled) throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(true);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.TrustedVaultKeyRequiredForEverything",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_trusted_vault_key_required_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForTrustedVaultKeyForPasswords(boolean nightModeEnabled)
            throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(false);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.TrustedVaultKeyRequiredForPasswords",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_trusted_vault_key_required_for_passwords_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForTrustedVaultRecoverabilityDegradedForEverything(
            boolean nightModeEnabled) throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(true);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(
                SyncSettingsUtils.SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.TrustedVaultRecoverabilityDegradedForEverything",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_trusted_vault_recoverability_degraded_for_everything_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForTrustedVaultRecoverabilityDegradedForPasswords(
            boolean nightModeEnabled) throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(false);
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(
                SyncSettingsUtils.SyncError.TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.TrustedVaultRecoverabilityDegradedForPasswords",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_trusted_vault_recoverability_degraded_for_passwords_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForSyncSetupIncomplete(boolean nightModeEnabled) throws Exception {
        // Passing a null SyncService instance here would sign-in the user but
        // FirstSetupComplete will be unset.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync(/* syncService= */ null);
        assertSyncError(SyncSettingsUtils.SyncError.SYNC_SETUP_INCOMPLETE);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.SyncSetupIncomplete",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(),
                "sync_error_card_sync_setup_incomplete_with_new_title");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForUpmBackendOutdated(boolean nightModeEnabled) throws Exception {
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        mSigninTestRule.addTestAccountThenSigninAndEnableSync(mFakeSyncServiceImpl);
        assertSyncError(SyncSettingsUtils.SyncError.UPM_BACKEND_OUTDATED);

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.SyncErrorCard.UpmBackendOutdated",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }

        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_upm_backend_outdated");
    }

    private View getPersonalizedSyncPromoView() {
        // Ensure that AccountInfoServiceProvider populated ProfileDataCache before checking the
        // view.
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccountInfoServiceProvider.getPromise()
                            .then(
                                    accountInfoService -> {
                                        callbackHelper.notifyCalled();
                                    });
                });
        try {
            callbackHelper.waitForOnly();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }

        ViewUtils.waitForVisibleView(withId(R.id.signin_promo_view_wrapper));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.signin_promo_view_wrapper);
                        });
        Assert.assertNotNull("No sync error card view found.", view);
        return view;
    }
}
