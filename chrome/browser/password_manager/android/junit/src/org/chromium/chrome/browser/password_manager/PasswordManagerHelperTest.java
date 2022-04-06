// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import com.google.common.base.Optional;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;

import java.util.Collections;

/** Tests for password manager helper methods. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowSystemClock.class, ShadowRecordHistogram.class})
@Batch(Batch.PER_CLASS)
public class PasswordManagerHelperTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private static final String ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Latency";
    private static final String ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Success";
    private static final String ACCOUNT_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.GetIntent.Error";
    private static final String ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.Account.Launch.Success";

    private static final String LOCAL_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Latency";
    private static final String LOCAL_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Success";
    private static final String LOCAL_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.GetIntent.Error";
    private static final String LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.CredentialManager.LocalProfile.Launch.Success";

    private static final String PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Latency";
    private static final String PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Success";
    private static final String PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM =
            "PasswordManager.PasswordCheckup.GetIntent.Error";
    private static final String PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM =
            "PasswordManager.PasswordCheckup.Launch.Success";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private CredentialManagerLauncher mCredentialManagerLauncherMock;

    @Mock
    private PasswordCheckupClientHelper mPasswordCheckupClientHelperMock;

    @Mock
    private SyncService mSyncServiceMock;

    @Mock
    private SettingsLauncher mSettingsLauncherMock;

    @Mock
    private PendingIntent mPendingIntentMock;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
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
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
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
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(false);
        Assert.assertTrue(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    public void testActivelySyncingPasswordsWithCustomPassphrase() {
        when(mSyncServiceMock.hasSyncConsent()).thenReturn(true);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.isEngineInitialized()).thenReturn(true);
        when(mSyncServiceMock.isUsingExplicitPassphrase()).thenReturn(true);
        Assert.assertFalse(
                PasswordManagerHelper.isSyncingPasswordsWithNoCustomPassphrase(mSyncServiceMock));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesCredentialManagerSync() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForAccount(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesCredentialManagerForLocal() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        verify(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForLocal(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class), any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForAccountIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForAccountIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForAccount(CredentialManagerError.API_ERROR);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM, CredentialManagerError.API_ERROR));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsMetricsWhenAccountIntentFails() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulIntentFetchingForAccount();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACCOUNT_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        ACCOUNT_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForLocalIntent() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        setUpSuccessfulIntentFetchingForLocal();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(LOCAL_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForLocalIntent() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        returnErrorWhenFetchingIntentForLocal(CredentialManagerError.API_ERROR);

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_GET_INTENT_ERROR_HISTOGRAM, CredentialManagerError.API_ERROR));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOCAL_GET_INTENT_LATENCY_HISTOGRAM));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsMetricsWhenLocalIntentFails() throws CanceledException {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        setUpSuccessfulIntentFetchingForLocal();
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.showPasswordSettings(ContextUtils.getApplicationContext(),
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(LOCAL_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOCAL_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testDoesntCallIntentIfFeatureIsDisabled() throws CanceledException {
        Context contextMock = Mockito.mock(Context.class);
        final String passwordSettingsFragment =
                "org.chromium.chrome.browser.password_manager.settings.PasswordSettings";
        Intent dummyIntent = new Intent();
        when(mSettingsLauncherMock.createSettingsActivityIntent(
                     eq(contextMock), eq(passwordSettingsFragment), any(Bundle.class)))
                .thenReturn(dummyIntent);
        setUpSuccessfulIntentFetchingForAccount();
        PasswordManagerHelper.showPasswordSettings(contextMock,
                ManagePasswordsReferrer.CHROME_SETTINGS, mSettingsLauncherMock,
                mCredentialManagerLauncherMock, mSyncServiceMock);
        verify(contextMock).startActivity(eq(dummyIntent));
        verify(mPendingIntentMock, never()).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesPasswordCheckupSync() {
        chooseToSyncPasswordsWithoutCustomPassphrase();

        PasswordManagerHelper.showPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, mSyncServiceMock);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupPendingIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.of(TEST_EMAIL_ADDRESS)), any(Callback.class),
                        any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testLaunchesPasswordCheckupForLocal() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(false);
        PasswordManagerHelper.showPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, mSyncServiceMock);

        verify(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupPendingIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.absent()), any(Callback.class), any(Callback.class));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testPasswordCheckupIntentCalledIfSuccess() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        PasswordManagerHelper.showPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, mSyncServiceMock);
        verify(mPendingIntentMock).send();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsSuccessMetricsForPasswordCheckupIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);

        PasswordManagerHelper.showPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, mSyncServiceMock);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsErrorMetricsForPasswordCheckupIntent() {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        returnErrorWhenFetchingIntentForPasswordCheckup(CredentialManagerError.API_ERROR);

        PasswordManagerHelper.showPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, mSyncServiceMock);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM,
                        CredentialManagerError.API_ERROR));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM, 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testRecordsMetricsWhenPasswordCheckupIntentFails() throws CanceledException {
        chooseToSyncPasswordsWithoutCustomPassphrase();
        setUpSuccessfulCheckupIntentFetching(mPendingIntentMock);
        doThrow(CanceledException.class).when(mPendingIntentMock).send();

        PasswordManagerHelper.showPasswordCheckup(PasswordCheckReferrer.SAFETY_CHECK,
                mPasswordCheckupClientHelperMock, mSyncServiceMock);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_LATENCY_HISTOGRAM, 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_SUCCESS_HISTOGRAM, 1));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PASSWORD_CHECKUP_GET_INTENT_ERROR_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        PASSWORD_CHECKUP_LAUNCH_CREDENTIAL_MANAGER_SUCCESS_HISTOGRAM, 0));
    }

    private void chooseToSyncPasswordsWithoutCustomPassphrase() {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncServiceMock.getChosenDataTypes())
                .thenReturn(CollectionUtil.newHashSet(ModelType.PASSWORDS));
        when(mSyncServiceMock.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL_ADDRESS, "0"));
    }

    private void setUpSuccessfulIntentFetchingForAccount() {
        doAnswer(invocation -> {
            Callback<PendingIntent> cb = invocation.getArgument(2);
            cb.onResult(mPendingIntentMock);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForAccount(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void setUpSuccessfulCheckupIntentFetching(PendingIntent intent) {
        doAnswer(invocation -> {
            Callback<PendingIntent> cb = invocation.getArgument(2);
            cb.onResult(intent);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupPendingIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.of(TEST_EMAIL_ADDRESS)), any(Callback.class),
                        any(Callback.class));
    }

    private void setUpSuccessfulIntentFetchingForLocal() {
        doAnswer(invocation -> {
            Callback<PendingIntent> cb = invocation.getArgument(1);
            cb.onResult(mPendingIntentMock);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForLocal(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForAccount(@CredentialManagerError int error) {
        doAnswer(invocation -> {
            Callback<Integer> cb = invocation.getArgument(3);
            cb.onResult(error);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForAccount(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        eq(TEST_EMAIL_ADDRESS), any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForLocal(@CredentialManagerError int error) {
        doAnswer(invocation -> {
            Callback<Integer> cb = invocation.getArgument(2);
            cb.onResult(error);
            return true;
        })
                .when(mCredentialManagerLauncherMock)
                .getCredentialManagerIntentForLocal(eq(ManagePasswordsReferrer.CHROME_SETTINGS),
                        any(Callback.class), any(Callback.class));
    }

    private void returnErrorWhenFetchingIntentForPasswordCheckup(
            @CredentialManagerError int error) {
        doAnswer(invocation -> {
            Callback<Integer> cb = invocation.getArgument(3);
            cb.onResult(error);
            return true;
        })
                .when(mPasswordCheckupClientHelperMock)
                .getPasswordCheckupPendingIntent(eq(PasswordCheckReferrer.SAFETY_CHECK),
                        eq(Optional.of(TEST_EMAIL_ADDRESS)), any(Callback.class),
                        any(Callback.class));
    }
}
