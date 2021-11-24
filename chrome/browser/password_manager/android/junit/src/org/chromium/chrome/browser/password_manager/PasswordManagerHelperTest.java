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
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
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
    private SyncService mSyncServiceMock;

    @Mock
    private SettingsLauncher mSettingsLauncherMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSyncCheckFeatureNotEnabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testSyncCheckNoSyncConsent() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(false);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsDisabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes()).thenReturn(Collections.EMPTY_SET);
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testSyncPasswordsEnabled() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        Assert.assertTrue(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
    }

    @Test
    public void testSyncEnabledWithCustomPassphrase() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        Assert.assertTrue(PasswordManagerHelper.hasChosenToSyncPasswords(mSyncServiceMock));
        Assert.assertFalse(PasswordManagerHelper.hasChosenToSyncPasswordsWithNoCustomPassphrase(
                mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithNoCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(false);
        Assert.assertTrue(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testLaunchesCredentialManagerSync() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerLaunchIntentForAccount(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS), eq(TEST_EMAIL_ADDRESS),
                        any(Callback.class), any(Callback.class));
    }

    @Test
    public void testLaunchesCredentialManagerForLocal() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerLaunchIntentForLocal(
                        eq(ManagePasswordsReferrer.CHROME_SETTINGS), any(Callback.class),
                        any(Callback.class));
    }
}