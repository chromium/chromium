// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;
import org.chromium.google_apis.gaia.GoogleServiceAuthErrorState;

/** Unit tests for {@link SyncSettingsUtils}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SyncSettingsUtilsTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;

    @Mock private SyncService mSyncService;

    @Mock private IdentityServicesProvider mIdentityServicesProvider;

    @Mock private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        // Default mock behavior to simulate no error
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        Mockito.when(mIdentityServicesProvider.getIdentityManager(mProfile))
                .thenReturn(mIdentityManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        Mockito.when(mSyncService.isInitialSyncFeatureSetupComplete()).thenReturn(true);
        Mockito.when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        Mockito.when(mSyncService.isTrustedVaultKeyRequired()).thenReturn(false);
        Mockito.when(mSyncService.isTrustedVaultRecoverabilityDegraded()).thenReturn(false);
        Mockito.when(mSyncService.requiresClientUpgrade()).thenReturn(false);
        Mockito.when(mSyncService.isEngineInitialized()).thenReturn(true);
        Mockito.when(mSyncService.getAuthError())
                .thenReturn(new GoogleServiceAuthError(GoogleServiceAuthErrorState.NONE));
    }

    @Test
    @SmallTest
    public void testGetSyncError_NoError() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.NO_ERROR,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_AuthError() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(true);
        Mockito.when(mSyncService.getAuthError())
                .thenReturn(
                        new GoogleServiceAuthError(
                                GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.AUTH_ERROR,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_TrustedVaultKeyRequired() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(true);
        Mockito.when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes())
                .thenReturn(true);
        Mockito.when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_TrustedVaultRecoverabilityDegraded() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(true);
        Mockito.when(mSyncService.isTrustedVaultRecoverabilityDegraded()).thenReturn(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError
                                    .TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_setupInProgress() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(true);
        Mockito.when(mSyncService.isInitialSyncFeatureSetupComplete()).thenReturn(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.SYNC_SETUP_INCOMPLETE,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_unrecoverableError() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(true);
        Mockito.when(mSyncService.hasUnrecoverableError()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.OTHER_ERRORS,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_noSyncConsent_authError() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(false);
        Mockito.when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        Mockito.when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        Mockito.when(mSyncService.getAuthError())
                .thenReturn(
                        new GoogleServiceAuthError(
                                GoogleServiceAuthErrorState.INVALID_GAIA_CREDENTIALS));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.AUTH_ERROR,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }

    @Test
    @SmallTest
    public void testGetSyncError_noSyncConsent_unrecoverableError() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(false);
        Mockito.when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        Mockito.when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        Mockito.when(mSyncService.hasUnrecoverableError()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.NO_ERROR,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }
    @Test
    @SmallTest
    public void testGetSyncError_noSyncConsent_notSignedIn() {
        Mockito.when(mSyncService.hasSyncConsent()).thenReturn(false);
        Mockito.when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        Mockito.when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            SyncSettingsUtils.SyncError.NO_ERROR,
                            SyncSettingsUtils.getSyncError(mProfile));
                });
    }
}
