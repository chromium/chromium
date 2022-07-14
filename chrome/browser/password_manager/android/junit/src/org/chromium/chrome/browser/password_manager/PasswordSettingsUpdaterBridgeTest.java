// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.accounts.Account;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.Status;
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
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.AccountUtils;

import java.util.OptionalInt;

/**
 * Tests that bridge calls invoked by the settings updater call the accessor and invoke the right
 * callbacks in return.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowSystemClock.class})
@Batch(Batch.PER_CLASS)
public class PasswordSettingsUpdaterBridgeTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final long sDummyNativePointer = 7;
    private static final String sTestAccountEmail = "test@email.com";
    private static final Optional<Account> sTestAccount =
            Optional.of(AccountUtils.createAccountFromName(sTestAccountEmail));
    private static final String HISTOGRAM_NAME_BASE = "PasswordManager.PasswordSettings";

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private PasswordSettingsUpdaterBridge.Natives mBridgeJniMock;
    @Mock
    private PasswordSettingsAccessor mAccessorMock;

    private PasswordSettingsUpdaterBridge mBridge;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordSettingsUpdaterBridgeJni.TEST_HOOKS, mBridgeJniMock);
        mBridge = new PasswordSettingsUpdaterBridge(sDummyNativePointer, mAccessorMock);
    }

    private void checkSuccessHistograms(String functionSuffix, String settingSuffix) {
        final String nameWithSuffixes =
                HISTOGRAM_NAME_BASE + "." + functionSuffix + "." + settingSuffix;
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Success", 1));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Latency", 0));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        nameWithSuffixes + ".ErrorLatency"));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".ErrorCode"));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".APIError1"));
    }

    private void checkFailureHistograms(
            String functionSuffix, String settingSuffix, int errorCode, OptionalInt apiErrorCode) {
        final String nameWithSuffixes =
                HISTOGRAM_NAME_BASE + "." + functionSuffix + "." + settingSuffix;
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(nameWithSuffixes + ".Success", 0));
        assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(nameWithSuffixes + ".Latency"));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + ".ErrorLatency", 0));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        nameWithSuffixes + ".ErrorCode", errorCode));
        apiErrorCode.ifPresentOrElse(apiError
                -> assertEquals(1,
                        RecordHistogram.getHistogramValueCountForTesting(
                                nameWithSuffixes + ".APIError1", apiError)),
                ()
                        -> assertEquals(0,
                                RecordHistogram.getHistogramTotalCountForTesting(
                                        nameWithSuffixes + ".APIError1")));
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

        checkSuccessHistograms("GetSettingValue", "OfferToSavePasswords");
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

        checkSuccessHistograms("GetSettingValue", "OfferToSavePasswords");
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

        checkFailureHistograms("GetSettingValue", "OfferToSavePasswords",
                AndroidBackendErrorType.UNCATEGORIZED, OptionalInt.empty());
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

        checkSuccessHistograms("GetSettingValue", "AutoSignIn");
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

        checkSuccessHistograms("GetSettingValue", "AutoSignIn");
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

        checkFailureHistograms("GetSettingValue", "AutoSignIn",
                AndroidBackendErrorType.UNCATEGORIZED, OptionalInt.empty());
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

        checkSuccessHistograms("SetSettingValue", "OfferToSavePasswords");
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

        checkFailureHistograms("SetSettingValue", "OfferToSavePasswords",
                AndroidBackendErrorType.UNCATEGORIZED, OptionalInt.empty());
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

        checkSuccessHistograms("SetSettingValue", "AutoSignIn");
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

        checkFailureHistograms("SetSettingValue", "AutoSignIn",
                AndroidBackendErrorType.UNCATEGORIZED, OptionalInt.empty());
    }

    @Test
    public void testSetAutoSignInSettingFailsWithAPIError() {
        mBridge.setSettingValue(sTestAccountEmail, PasswordManagerSetting.AUTO_SIGN_IN, true);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mAccessorMock)
                .setAutoSignIn(eq(true), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception expectedException =
                new ApiException(new Status(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
        failureCallback.getValue().onResult(expectedException);
        verify(mBridgeJniMock)
                .onFailedSettingChange(sDummyNativePointer, PasswordManagerSetting.AUTO_SIGN_IN,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE);

        checkFailureHistograms("SetSettingValue", "AutoSignIn",
                AndroidBackendErrorType.EXTERNAL_ERROR,
                OptionalInt.of(ChromeSyncStatusCode.AUTH_ERROR_UNRESOLVABLE));
    }
}
