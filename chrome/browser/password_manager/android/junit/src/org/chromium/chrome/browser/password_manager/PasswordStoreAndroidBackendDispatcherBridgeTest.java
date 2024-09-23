// Copyright 2021 The Chromium Authors
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

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.ResolvableApiException;
import com.google.android.gms.common.api.Status;

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
import org.chromium.components.password_manager.core.browser.proto.ListAffiliatedPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.ListAffiliatedPasswordsResult.AffiliatedPassword;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsWithUiInfoResult;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsWithUiInfoResult.PasswordWithUiInfo;
import org.chromium.components.password_manager.core.browser.proto.PasswordWithLocalData;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.sync.protocol.PasswordSpecificsData;

import java.util.Optional;

/** Tests that dispatcher bridge calls as invoked by the password store reach the backend. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordStoreAndroidBackendDispatcherBridgeTest {

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
    private static final String sTestAccountEmail = "test@email.com";
    private static final Optional<Account> sTestAccount =
            Optional.of(AccountUtils.createAccountFromName(sTestAccountEmail));

    @Mock private PasswordStoreAndroidBackendReceiverBridgeImpl mBackendReceiverBridgeMock;
    @Mock private PasswordStoreAndroidBackend mBackendMock;

    private PasswordStoreAndroidBackendDispatcherBridgeImpl mBackendDispatcherBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBackendDispatcherBridge =
                new PasswordStoreAndroidBackendDispatcherBridgeImpl(
                        mBackendReceiverBridgeMock, mBackendMock);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendDispatcherBridge.getAllLogins(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        byte[] kExpectedList = sTestLogins.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBackendReceiverBridgeMock).onCompleteWithLogins(kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnUncategorizedFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAllLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(Optional.empty()), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnPreconditionFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAllLogins(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException =
                new PasswordStoreAndroidBackend.BackendException(
                        "Sample failure", AndroidBackendErrorType.NO_ACCOUNT);
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testGetAllLoginsCallsBridgeOnAPIFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAllLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(Optional.empty()), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException =
                new ApiException(
                        new Status(new ConnectionResult(ConnectionResult.API_UNAVAILABLE), ""));
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testGetAllLoginsWithBrandingInfoOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendDispatcherBridge.getAllLoginsWithBrandingInfo(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAllLoginsWithBrandingInfo(eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        PasswordWithUiInfo password =
                PasswordWithUiInfo.newBuilder().setPasswordData(sTestPwdWithLocalData).build();
        ListPasswordsWithUiInfoResult.Builder passwordsResult =
                ListPasswordsWithUiInfoResult.newBuilder().addPasswordsWithUiInfo(password);
        byte[] kExpectedList = passwordsResult.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBackendReceiverBridgeMock).onCompleteWithBrandedLogins(kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetAllLoginsWithBrandingInfoFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAllLoginsWithBrandingInfo(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAllLoginsWithBrandingInfo(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testDoesNotStartResolutionOnAPIFailure() throws PendingIntent.CanceledException {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAllLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).getAllLogins(eq(Optional.empty()), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        Exception kExpectedException =
                new ResolvableApiException(
                        new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));
        failureCallback.getValue().onResult(kExpectedException);
        verify(pendingIntentMock, never()).send();
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testGetAutofillableLoginsCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendDispatcherBridge.getAutofillableLogins(kTestTaskId, null);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAutofillableLogins(eq(Optional.empty()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        byte[] kExpectedList = sTestLogins.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBackendReceiverBridgeMock).onCompleteWithLogins(kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetAutofillableLoginsCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAutofillableLogins(kTestTaskId, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAutofillableLogins(eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testGetLoginsForSignonRealmCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendDispatcherBridge.getLoginsForSignonRealm(
                kTestTaskId, "https://test_signon_realm.com", null);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getLoginsForSignonRealm(
                        any(), eq(Optional.empty()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        byte[] kExpectedList = sTestLogins.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBackendReceiverBridgeMock).onCompleteWithLogins(kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetLoginsForSignonRealmCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getLoginsForSignonRealm(
                kTestTaskId, "https://test_signon_realm.com", sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getLoginsForSignonRealm(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testGetAffiliatedLoginsForSignonRealmOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        mBackendDispatcherBridge.getAffiliatedLoginsForSignonRealm(
                kTestTaskId, "https://test_signon_realm.com", sTestAccountEmail);
        ArgumentCaptor<Callback<byte[]>> successCallback = ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAffiliatedLoginsForSignonRealm(
                        any(), eq(sTestAccount), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        AffiliatedPassword affiliatedPassword =
                AffiliatedPassword.newBuilder().setPasswordData(sTestPwdWithLocalData).build();
        ListAffiliatedPasswordsResult.Builder affiliatedPasswordsResult =
                ListAffiliatedPasswordsResult.newBuilder()
                        .addAffiliatedPasswords(affiliatedPassword);
        byte[] kExpectedList = affiliatedPasswordsResult.build().toByteArray();
        successCallback.getValue().onResult(kExpectedList);
        verify(mBackendReceiverBridgeMock)
                .onCompleteWithAffiliatedLogins(kTestTaskId, kExpectedList);
    }

    @Test
    public void testGetAffiliatedLoginsForSignonRealmOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        mBackendDispatcherBridge.getAffiliatedLoginsForSignonRealm(
                kTestTaskId, "https://test_signon_realm.com", sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock)
                .getAffiliatedLoginsForSignonRealm(
                        any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testAddLoginCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendDispatcherBridge.addLogin(kTestTaskId, pwdWithLocalData, null);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock)
                .addLogin(any(), eq(Optional.empty()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBackendReceiverBridgeMock).onLoginChanged(kTestTaskId);
    }

    @Test
    public void testAddLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendDispatcherBridge.addLogin(kTestTaskId, pwdWithLocalData, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).addLogin(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testUpdateLoginCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendDispatcherBridge.updateLogin(kTestTaskId, pwdWithLocalData, null);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock)
                .updateLogin(any(), eq(Optional.empty()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBackendReceiverBridgeMock).onLoginChanged(kTestTaskId);
    }

    @Test
    public void testUpdateLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdWithLocalData = sTestPwdWithLocalData.build().toByteArray();
        mBackendDispatcherBridge.updateLogin(kTestTaskId, pwdWithLocalData, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).updateLogin(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }

    @Test
    public void testRemoveLoginCallsBridgeOnSuccess() {
        final int kTestTaskId = 1337;

        // Ensure the backend is called with a valid success callback.
        byte[] pwdSpecificsData = sTestProfile.build().toByteArray();
        mBackendDispatcherBridge.removeLogin(kTestTaskId, pwdSpecificsData, null);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mBackendMock)
                .removeLogin(any(), eq(Optional.empty()), successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBackendReceiverBridgeMock).onLoginChanged(kTestTaskId);
    }

    @Test
    public void testRemoveLoginCallsBridgeOnFailure() {
        final int kTestTaskId = 42069;

        // Ensure the backend is called with a valid failure callback.
        byte[] pwdSpecificsData = sTestProfile.build().toByteArray();
        mBackendDispatcherBridge.removeLogin(kTestTaskId, pwdSpecificsData, sTestAccountEmail);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mBackendMock).removeLogin(any(), eq(sTestAccount), any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        Exception kExpectedException = new Exception("Sample failure");
        failureCallback.getValue().onResult(kExpectedException);
        verify(mBackendReceiverBridgeMock)
                .handleAndroidBackendException(kTestTaskId, kExpectedException);
    }
}
