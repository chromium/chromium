// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;

/**
 * Test suite for SyncErrorCardPreference
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
@DisableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class SyncErrorCardPreferenceTest {
    // FakeProfileDataSource is required to create the ProfileDataCache entry with sync_error badge
    // for Sync error card.
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().setRevision(4).build();

    private FakeProfileSyncService mFakeProfileSyncService;

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
    public void setUp() throws Exception {
        // Start main activity before because native side needs to be initialized before overriding
        // ProfileSyncService.
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeProfileSyncService = new FakeProfileSyncService();
            ProfileSyncService.overrideForTests(mFakeProfileSyncService);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ProfileSyncService.resetForTests();
        });
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForAndroidSyncDisabled(boolean nightModeEnabled) throws Exception {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mFakeProfileSyncService);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeProfileSyncService.setSyncAllowedByPlatform(false);

            Assert.assertEquals("ANDROID_SYNC_DISABLED SyncError should be set",
                    SyncSettingsUtils.SyncError.ANDROID_SYNC_DISABLED,
                    SyncSettingsUtils.getSyncError());
        });

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_android_sync_disabled");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForAuthError(boolean nightModeEnabled) throws Exception {
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mFakeProfileSyncService);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("AUTH_ERROR SyncError should be set",
                                SyncSettingsUtils.SyncError.AUTH_ERROR,
                                SyncSettingsUtils.getSyncError()));

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(getPersonalizedSyncPromoView(), "sync_error_card_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForClientOutOfDate(boolean nightModeEnabled) throws Exception {
        mFakeProfileSyncService.setRequiresClientUpgrade(true);
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mFakeProfileSyncService);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("CLIENT_OUT_OF_DATE SyncError should be set",
                                SyncSettingsUtils.SyncError.CLIENT_OUT_OF_DATE,
                                SyncSettingsUtils.getSyncError()));

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_client_out_of_date");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForOtherErrors(boolean nightModeEnabled) throws Exception {
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.CONNECTION_FAILED);
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mFakeProfileSyncService);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("OTHER_ERRORS SyncError should be set",
                                SyncSettingsUtils.SyncError.OTHER_ERRORS,
                                SyncSettingsUtils.getSyncError()));

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(getPersonalizedSyncPromoView(), "sync_error_card_other_errors");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForPassphraseRequired(boolean nightModeEnabled) throws Exception {
        mFakeProfileSyncService.setEngineInitialized(true);
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(true);
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mFakeProfileSyncService);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("PASSPHRASE_REQUIRED SyncError should be set",
                                SyncSettingsUtils.SyncError.PASSPHRASE_REQUIRED,
                                SyncSettingsUtils.getSyncError()));

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_passphrase_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForTrustedVaultKey(boolean nightModeEnabled) throws Exception {
        mFakeProfileSyncService.setEngineInitialized(true);
        mFakeProfileSyncService.setTrustedVaultKeyRequiredForPreferredDataTypes(true);
        mFakeProfileSyncService.setEncryptEverythingEnabled(true);
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mFakeProfileSyncService);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("TRUSTED_VAULT_KEY_REQUIRED SyncError should be set",
                                SyncSettingsUtils.SyncError
                                        .TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
                                SyncSettingsUtils.getSyncError()));

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_trusted_vault_key_required");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSyncErrorCardForSyncSetupIncomplete(boolean nightModeEnabled) throws Exception {
        // Passing a null ProfileSyncService instance here would sign-in the user but
        // FirstSetupComplete will be unset.
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(
                /* profileSyncService= */ null);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("SYNC_SETUP_INCOMPLETE SyncError should be set",
                                SyncSettingsUtils.SyncError.SYNC_SETUP_INCOMPLETE,
                                SyncSettingsUtils.getSyncError()));

        mSettingsActivityTestRule.startSettingsActivity();
        mRenderTestRule.render(
                getPersonalizedSyncPromoView(), "sync_error_card_sync_setup_incomplete");
    }

    private View getPersonalizedSyncPromoView() {
        return mSettingsActivityTestRule.getActivity().findViewById(
                R.id.signin_promo_view_container);
    }
}
