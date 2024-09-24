// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting;

import android.accounts.Account;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.signin.AccountUtils;

import java.util.Optional;

/** Tests that bridge calls invoked by the settings updater call the accessor. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordSettingsUpdaterDispatcherBridgeTest {

    private static final String sTestAccountEmail = "test@email.com";
    private static final Optional<Account> sTestAccount =
            Optional.of(AccountUtils.createAccountFromName(sTestAccountEmail));

    @Mock private PasswordSettingsAccessor mAccessorMock;
    @Mock private PasswordSettingsUpdaterReceiverBridge mReceiverBridgeMock;

    private PasswordSettingsUpdaterDispatcherBridge mDispatcherBridge;

    @Before
    public void setUp() {
        // Dispatcher bridge checks it is used on the background thread. Disable this check for this
        // test.
        hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        MockitoAnnotations.initMocks(this);
        mDispatcherBridge =
                new PasswordSettingsUpdaterDispatcherBridge(mReceiverBridgeMock, mAccessorMock);
    }

    @Test
    public void testGetSavePasswordsSettingValueSucceeds() {
        mDispatcherBridge.getSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .getOfferToSavePasswords(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.of(true));

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueFetched(
                        eq(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS),
                        eq(Optional.of(true)),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testGetSavePasswordsSettingAbsentSucceeds() {
        mDispatcherBridge.getSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .getOfferToSavePasswords(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.empty());

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueFetched(
                        eq(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS),
                        eq(Optional.empty()),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testGetSavePasswordsSettingFails() {
        mDispatcherBridge.getSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .getOfferToSavePasswords(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .handleFetchingException(
                        eq(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS),
                        eq(expectedException),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testGetAutoSignInSettingValueSucceeds() {
        mDispatcherBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getAutoSignIn(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.of(true));

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueFetched(
                        eq(PasswordManagerSetting.AUTO_SIGN_IN),
                        eq(Optional.of(true)),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testGetAutoSignInAbsentSucceeds() {
        mDispatcherBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getAutoSignIn(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.empty());

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueFetched(
                        eq(PasswordManagerSetting.AUTO_SIGN_IN),
                        eq(Optional.empty()),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testGetAutoSignInSettingFails() {
        mDispatcherBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getAutoSignIn(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .handleFetchingException(
                        eq(PasswordManagerSetting.AUTO_SIGN_IN),
                        eq(expectedException),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testSetSavePasswordsSucceeds() {
        mDispatcherBridge.setSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
        ArgumentCaptor<Callback<Void>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setOfferToSavePasswords(
                        eq(true), eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(null);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueSet(
                        eq(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "SetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testSetSavePasswordsSettingFails() {
        mDispatcherBridge.setSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setOfferToSavePasswords(
                        eq(true), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .handleSettingException(
                        eq(PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS),
                        eq(expectedException),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "SetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testSetAutoSignInSucceeds() {
        mDispatcherBridge.setSettingValue(
                sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN, true);
        ArgumentCaptor<Callback<Void>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setAutoSignIn(eq(true), eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(null);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueSet(
                        eq(PasswordManagerSetting.AUTO_SIGN_IN), metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "SetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testSetAutoSignInSettingFails() {
        mDispatcherBridge.setSettingValue(
                sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN, true);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setAutoSignIn(eq(true), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .handleSettingException(
                        eq(PasswordManagerSetting.AUTO_SIGN_IN),
                        eq(expectedException),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "SetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testGetBiometricReauthBeforePwdFillingSucceeds() {
        mDispatcherBridge.getSettingValue(
                sTestAccountEmail, PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getUseBiometricReauthBeforeFilling(successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.of(true));

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueFetched(
                        eq(PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING),
                        eq(Optional.of(true)),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING);
    }

    @Test
    public void testGetBiometricReauthBeforePwdFillingAbsentSucceeds() {
        mDispatcherBridge.getSettingValue(
                sTestAccountEmail, PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getUseBiometricReauthBeforeFilling(successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.empty());

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .onSettingValueFetched(
                        eq(PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING),
                        eq(Optional.empty()),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING);
    }

    @Test
    public void testGetBiometricReauthBeforePwdFillingFails() {
        mDispatcherBridge.getSettingValue(
                sTestAccountEmail, PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getUseBiometricReauthBeforeFilling(any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);

        ArgumentCaptor<PasswordSettingsUpdaterMetricsRecorder> metricsRecorder =
                ArgumentCaptor.forClass(PasswordSettingsUpdaterMetricsRecorder.class);
        verify(mReceiverBridgeMock)
                .handleFetchingException(
                        eq(PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING),
                        eq(expectedException),
                        metricsRecorder.capture());

        assertEquals(metricsRecorder.getValue().getFunctionSuffixForTesting(), "GetSettingValue");
        assertEquals(
                metricsRecorder.getValue().getSettingForTesting(),
                PasswordManagerSetting.BIOMETRIC_REAUTH_BEFORE_PWD_FILLING);
    }
}
