// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.accounts.Account;
import android.app.PendingIntent;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.ResolvableApiException;
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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.PasswordWithLocalData;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.sync.protocol.PasswordSpecificsData;

import java.util.Date;

/**
 * Tests that bridge calls as invoked by the password store reach the backend and return correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
public class PasswordStoreAndroidBackendBridgeTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final PasswordSpecificsData.Builder sTestProfile =
            PasswordSpecificsData.newBuilder()
                    .setUsernameValue("Todd Tester")
                    .setUsernameElement("name")
                    .setPasswordElement("pwd")
                    .setOrigin("https://www.google.com/")
                    .setSignonRealm("https://accounts.google.com/signin")
                    .setPasswordValue("S3cr3t");
    private static final PasswordWithLocalData.Builder sTestPwdWithLocalData =
            PasswordWithLocalData.newBuilder().setPasswordSpecificsData(sTestProfile);
    private static final ListPasswordsResult.Builder sTestLogins =
            ListPasswordsResult.newBuilder().addPasswordData(sTestPwdWithLocalData);
    private static final long sDummyNativePointer = 4;
    private static final String sTestAccountEmail = "test@email.com";
    private static final Optional<Account> sTestAccount =
            Optional.of(AccountUtils.createAccountFromName(sTestAccountEmail));

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private PasswordStoreAndroidBackendBridgeImpl.Natives mBridgeJniMock;
    @Mock
    private PasswordStoreAndroidBackend mBackendMock;

    private PasswordStoreAndroidBackendBridgeImpl mBackendBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordStoreAndroidBackendBridgeImplJni.TEST_HOOKS, mBridgeJniMock);
        mBackendBridge =
                new PasswordStoreAndroidBackendBridgeImpl(sDummyNativePointer, mBackendMock);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendBridge.getAllLogins(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        byte[] kExpectedList = sTestLogins.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBridgeJniMock)
                .onCompleteWithLogins(sDummyNativePointer, kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnUncategorizedFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.getAllLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(Optional.absent()), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnPreconditionFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.getAllLogins(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new PasswordStoreAndroidBackend.BackendException(
                "Sample failure", AndroidBackendErrorType.NO_ACCOUNT);
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.NO_ACCOUNT, 0);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnAPIFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.getAllLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(Optional.absent()), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new ApiException(new Status(CommonStatusCodes.ERROR, ""));
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.ERROR);
    }

    @Test
    public void testSubscribeCallsApiWithMinimalListCallAndForwardsErrorStateToBridge() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.subscribe(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAllLoginsBetween(eq(new Date(1)), eq(new Date(2)), eq(Optional.absent()), any(),
                        failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new ApiException(new Status(CommonStatusCodes.ERROR, ""));
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onSubscribeFailed(sDummyNativePointer, kTestTaskId,
                        AndroidBackendErrorType.EXTERNAL_ERROR, CommonStatusCodes.ERROR);
    }

    @Test
    public void testSubscribeCallsApiWithMinimalListCallAndForwardsSuccessToBridge() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid success callback.
        mBackendBridge.subscribe(kTestTaskId, null);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAllLoginsBetween(eq(new Date(1)), eq(new Date(2)), eq(Optional.absent()),
                        successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().onResult(sTestLogins.build().toByteArray());
        verify(mBridgeJniMock).onSubscribed(sDummyNativePointer, kTestTaskId);
    }

    @Test
    public void testDoesNotStartResolutionOnAPIFailure() throws PendingIntent.CanceledException {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.getAllLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(Optional.absent()), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception kExpectedException = new ResolvableApiException(
                new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));
        failureCallback.getValue().onResult(kExpectedException);
        verify(pendingIntentMock, never()).send();
        verify(mBridgeJniMock)
                .onError(sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.RESOLUTION_REQUIRED);
    }

    @Test
    public void testGetAutofillableLoginsCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendBridge.getAutofillableLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAutofillableLogins(eq(Optional.absent()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        byte[] kExpectedList = sTestLogins.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBridgeJniMock)
                .onCompleteWithLogins(sDummyNativePointer, kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetAutofillableLoginsCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.getAutofillableLogins(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAutofillableLogins(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testGetLoginsForSignonRealmCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendBridge.getLoginsForSignonRealm(kTestTaskId, "https://test_signon_realm.com", null);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getLoginsForSignonRealm(
                        any(), eq(Optional.absent()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        byte[] kExpectedList = sTestLogins.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBridgeJniMock)
                .onCompleteWithLogins(sDummyNativePointer, kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetLoginsForSignonRealmCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendBridge.getLoginsForSignonRealm(
                kTestTaskId, "https://test_signon_realm.com", sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getLoginsForSignonRealm(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testAddLoginCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendBridge.addLogin(kTestTaskId, pwdWithLocalData, null);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock)
                .addLogin(any(), eq(Optional.absent()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onLoginChanged(sDummyNativePointer, kTestTaskId);
    }

    @Test
    public void testAddLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendBridge.addLogin(kTestTaskId, pwdWithLocalData, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).addLogin(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testUpdateLoginCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendBridge.updateLogin(kTestTaskId, pwdWithLocalData, null);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock)
                .updateLogin(any(), eq(Optional.absent()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onLoginChanged(sDummyNativePointer, kTestTaskId);
    }

    @Test
    public void testUpdateLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendBridge.updateLogin(kTestTaskId, pwdWithLocalData, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).updateLogin(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testRemoveLoginCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        byte[] pwdSpecificsData = sTestProfile.build().toByteArray();
        mBackendBridge.removeLogin(kTestTaskId, pwdSpecificsData, null);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock)
                .removeLogin(any(), eq(Optional.absent()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onLoginChanged(sDummyNativePointer, kTestTaskId);
    }

    @Test
    public void testRemoveLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdSpecificsData = sTestProfile.build().toByteArray();
        mBackendBridge.removeLogin(kTestTaskId, pwdSpecificsData, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).removeLogin(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }
}
