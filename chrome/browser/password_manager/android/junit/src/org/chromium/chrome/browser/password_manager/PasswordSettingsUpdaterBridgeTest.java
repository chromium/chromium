// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.accounts.Account;

import com.google.common.base.Optional;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.AccountUtils;

/**
 * Tests that bridge calls invoked by the settings updater call the accessor and invoke the right
 * callbacks in return.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordSettingsUpdaterBridgeTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final long sDummyNativePointer = 7;
    private static final String sTestAccountEmail = "test@email.com";
    private static final Optional<Account> sTestAccount =
            Optional.of(AccountUtils.createAccountFromName(sTestAccountEmail));

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private PasswordSettingsUpdaterBridge.Natives mBridgeJniMock;
    @Mock
    private PasswordSettingsAccessor mAccessorMock;

    private PasswordSettingsUpdaterBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordSettingsUpdaterBridgeJni.TEST_HOOKS, mBridgeJniMock);
        mBridge = new PasswordSettingsUpdaterBridge(sDummyNativePointer, mAccessorMock);
    }

    @Test
    public void testGetSavePasswordsSettingValueSucceeds() {
        mBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .getOfferToSavePasswords(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.of(true));
        verify(mBridgeJniMock)
                .onSettingValueFetched(
                        sDummyNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
    }

    @Test
    public void testGetSavePasswordsSettingAbsentSucceeds() {
        mBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .getOfferToSavePasswords(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.absent());
        verify(mBridgeJniMock)
                .onSettingValueAbsent(
                        sDummyNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testGetSavePasswordsSettingFails() {
        mBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .getOfferToSavePasswords(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);
        verify(mBridgeJniMock)
                .onSettingFetchingError(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testGetAutoSignInSettingValueSucceeds() {
        mBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getAutoSignIn(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.of(true));
        verify(mBridgeJniMock)
                .onSettingValueFetched(
                        sDummyNativePointer, PasswordManagerSetting.AUTO_SIGN_IN, true);
    }

    @Test
    public void testGetAutoSignInAbsentSucceeds() {
        mBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN);
        ArgumentCaptor<Callback<Optional<Boolean>>> successCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getAutoSignIn(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(Optional.absent());
        verify(mBridgeJniMock)
                .onSettingValueAbsent(sDummyNativePointer, PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testGetAutoSignInSettingFails() {
        mBridge.getSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock).getAutoSignIn(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);
        verify(mBridgeJniMock)
                .onSettingFetchingError(sDummyNativePointer, PasswordManagerSetting.AUTO_SIGN_IN,
                        AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testSetSavePasswordsSucceeds() {
        mBridge.setSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
        ArgumentCaptor<Callback<Void>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setOfferToSavePasswords(
                        eq(true), eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(null);
        verify(mBridgeJniMock)
                .onSuccessfulSettingChange(
                        sDummyNativePointer, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS);
    }

    @Test
    public void testSetSavePasswordsSettingFails() {
        mBridge.setSettingValue(
                sTestAccountEmail, PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS, true);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setOfferToSavePasswords(
                        eq(true), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);
        verify(mBridgeJniMock)
                .onFailedSettingChange(sDummyNativePointer,
                        PasswordManagerSetting.OFFER_TO_SAVE_PASSWORDS,
                        AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testSetAutoSignInSucceeds() {
        mBridge.setSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN, true);
        ArgumentCaptor<Callback<Void>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setAutoSignIn(eq(true), eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(null);
        verify(mBridgeJniMock)
                .onSuccessfulSettingChange(
                        sDummyNativePointer, PasswordManagerSetting.AUTO_SIGN_IN);
    }

    @Test
    public void testSetAutoSignInSettingFails() {
        mBridge.setSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN, true);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setAutoSignIn(eq(true), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(expectedException);
        verify(mBridgeJniMock)
                .onFailedSettingChange(sDummyNativePointer, PasswordManagerSetting.AUTO_SIGN_IN,
                        AndroidBackendErrorType.UNCATEGORIZED, 0);
    }
}
