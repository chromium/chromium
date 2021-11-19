// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.ModelType;

import java.util.Collections;

/** Tests for password manager helper methods. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordManagerHelperTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Mock
    private CredentialManagerLauncher mCredentialManagerLauncherMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private SyncService mSyncServiceMock;

    @Mock
    private SettingsLauncher mSettingsLauncherMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSyncCheckNoSyncConsent() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswords(mIdentityManagerMock, mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsDisabled() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes()).thenReturn(Collections.EMPTY_SET);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswords(mIdentityManagerMock, mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsEnabled() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        Assert.assertTrue(
                PasswordManagerHelper.isSyncingPasswords(mIdentityManagerMock, mSyncServiceMock));
    }

    @Test
    public void testSyncEnabledWithCustomPassphrase() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        Assert.assertTrue(
                PasswordManagerHelper.isSyncingPasswords(mIdentityManagerMock, mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.isSyncingPasswordsWithoutCustomPassphrase(
                mIdentityManagerMock, mSyncServiceMock));
    }

    @Test
    public void testLaunchesCredentialManagerSync() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mIdentityManagerMock, mSyncServiceMock);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerLaunchIntentForAccount(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS), eq(TEST_EMAIL_ADDRESS),
                        any(Callback.class), any(Callback.class));
    }

    @Test
    public void testLaunchesCredentialManagerForLocal() {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mIdentityManagerMock, mSyncServiceMock);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerLaunchIntentForLocal(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS), any(Callback.class),
                        any(Callback.class));
    }
}