// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;
import org.chromium.ui.test.util.ViewUtils;

/** Test suite for IdentityErrorCardPreference */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IdentityErrorCardPreferenceTest {
    public final SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private static final String RENDER_TEST_DESCRIPTION = "Identity error card.";
    private static final int RENDER_TEST_REVISION = 1;

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SYNC)
                    .build();

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    private FakeSyncServiceImpl mFakeSyncServiceImpl;

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);

        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(mFakeSyncServiceImpl);
                });
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForAuthError() throws Exception {
        mFakeSyncServiceImpl.setAuthError(
                new GoogleServiceAuthError(GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS));
        mSigninTestRule.addTestAccountThenSignin();

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.AuthError",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(getIdentityErrorCardView(), "identity_error_card_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForClientOutOfDate() throws Exception {
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);
        mSigninTestRule.addTestAccountThenSignin();

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.ClientOutOfDate",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(), "identity_error_card_client_out_of_date");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForPassphraseRequired() throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setPassphraseRequiredForPreferredDataTypes(true);
        mSigninTestRule.addTestAccountThenSignin();

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.PassphraseRequired",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(), "identity_error_card_passphrase_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForTrustedVaultKey() throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(true);
        mSigninTestRule.addTestAccountThenSignin();

        mSettingsActivityTestRule.startSettingsActivity();
        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.TrustedVaultKeyRequiredForEverything",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(), "identity_error_card_trusted_vault_key_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForTrustedVaultKeyForPasswords() throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(false);
        mSigninTestRule.addTestAccountThenSignin();

        mSettingsActivityTestRule.startSettingsActivity();
        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.TrustedVaultKeyRequiredForPasswords",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(),
                "identity_error_card_trusted_vault_key_required_for_passwords");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForTrustedVaultRecoverabilityDegradedForEverything()
            throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(true);
        mSigninTestRule.addTestAccountThenSignin();

        mSettingsActivityTestRule.startSettingsActivity();
        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.TrustedVaultRecoverabilityDegradedForEverything",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(),
                "identity_error_card_trusted_vault_recoverability_degraded_for_everything");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForTrustedVaultRecoverabilityDegradedForPasswords()
            throws Exception {
        mFakeSyncServiceImpl.setEngineInitialized(true);
        mFakeSyncServiceImpl.setTrustedVaultRecoverabilityDegraded(true);
        mFakeSyncServiceImpl.setEncryptEverythingEnabled(false);
        mSigninTestRule.addTestAccountThenSignin();

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.TrustedVaultRecoverabilityDegradedForPasswords",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(),
                "identity_error_card_trusted_vault_recoverability_degraded_for_passwords");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testIdentityErrorCardForUpmBackendOutdated() throws Exception {
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        mSigninTestRule.addTestAccountThenSignin();

        try (HistogramWatcher watchIdentityErrorCardShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Sync.IdentityErrorCard.UpmBackendOutdated",
                        SyncSettingsUtils.ErrorUiAction.SHOWN)) {
            mSettingsActivityTestRule.startSettingsActivity();
        }
        mRenderTestRule.render(
                getIdentityErrorCardView(), "identity_error_card_upm_backend_outdated");
    }

    @Test
    @LargeTest
    public void testIdentityErrorCardNotShownForUnrecoverableErrors() throws Exception {
        mFakeSyncServiceImpl.setAuthError(
                new GoogleServiceAuthError(GoogleServiceAuthErrorState.CONNECTION_FAILED));
        mSigninTestRule.addTestAccountThenSignin();

        mSettingsActivityTestRule.startSettingsActivity();
        onView(withId(R.id.signin_settings_card)).check(doesNotExist());
    }

    private View getIdentityErrorCardView() {
        ViewUtils.waitForVisibleView(withId(R.id.signin_settings_card));
        View view =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.signin_settings_card);
                        });
        Assert.assertNotNull("No identity error card view found.", view);
        return view;
    }
}
