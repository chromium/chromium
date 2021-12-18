// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.sync.protocol.ListPasswordsResult;
import org.chromium.components.sync.protocol.PasswordSpecificsData;
import org.chromium.components.sync.protocol.PasswordWithLocalData;

/**
 * Tests that bridge calls as invoked by the password store reach the backend and return correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordStoreAndroidBackendBridgeTest {
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
        mBackendBridge.getAllLogins(kTestTaskId, PasswordStoreOperationTarget.DEFAULT);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(successCallback.capture(), any());
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
        mBackendBridge.getAllLogins(kTestTaskId, PasswordStoreOperationTarget.DEFAULT);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(any(), failureCallback.capture());
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
        mBackendBridge.getAllLogins(kTestTaskId, PasswordStoreOperationTarget.DEFAULT);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(any(), failureCallback.capture());
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
        mBackendBridge.getAllLogins(kTestTaskId, PasswordStoreOperationTarget.DEFAULT);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException =
                new ApiException(new Status(CommonStatusCodes.RESOLUTION_REQUIRED, ""));
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.EXTERNAL_ERROR,
                        6);
    }

    @Test
    public void testGetAutofillableLoginsCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendBridge.getAutofillableLogins(kTestTaskId);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAutofillableLogins(successCallback.capture(), any());
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
        mBackendBridge.getAutofillableLogins(kTestTaskId);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAutofillableLogins(any(), failureCallback.capture());
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
        mBackendBridge.getLoginsForSignonRealm(kTestTaskId, "https://test_signon_realm.com");
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getLoginsForSignonRealm(any(), successCallback.capture(), any());
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
        mBackendBridge.getLoginsForSignonRealm(kTestTaskId, "https://test_signon_realm.com");
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getLoginsForSignonRealm(any(), any(), failureCallback.capture());
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
        mBackendBridge.addLogin(kTestTaskId, pwdWithLocalData);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock).addLogin(any(), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onLoginAdded(sDummyNativePointer, kTestTaskId, pwdWithLocalData);
    }

    @Test
    public void testAddLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendBridge.addLogin(kTestTaskId, pwdWithLocalData);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).addLogin(any(), any(), failureCallback.capture());
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
        mBackendBridge.updateLogin(kTestTaskId, pwdWithLocalData);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock).updateLogin(any(), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onLoginUpdated(sDummyNativePointer, kTestTaskId, pwdWithLocalData);
    }

    @Test
    public void testUpdateLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendBridge.updateLogin(kTestTaskId, pwdWithLocalData);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).updateLogin(any(), any(), failureCallback.capture());
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
        mBackendBridge.removeLogin(
                kTestTaskId, pwdSpecificsData, PasswordStoreOperationTarget.DEFAULT);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock).removeLogin(any(), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onLoginDeleted(sDummyNativePointer, kTestTaskId, pwdSpecificsData);
    }

    @Test
    public void testRemoveLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdSpecificsData = sTestProfile.build().toByteArray();
        mBackendBridge.removeLogin(
                kTestTaskId, pwdSpecificsData, PasswordStoreOperationTarget.DEFAULT);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).removeLogin(any(), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBridgeJniMock)
                .onError(
                        sDummyNativePointer, kTestTaskId, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }
}
