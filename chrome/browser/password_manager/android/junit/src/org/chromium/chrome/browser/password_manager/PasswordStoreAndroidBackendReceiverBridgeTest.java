// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.verify;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.password_manager.core.browser.proto.ListAffiliatedPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.ListAffiliatedPasswordsResult.AffiliatedPassword;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsResult;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsWithUiInfoResult;
import org.chromium.components.password_manager.core.browser.proto.ListPasswordsWithUiInfoResult.PasswordWithUiInfo;
import org.chromium.components.password_manager.core.browser.proto.PasswordWithLocalData;
import org.chromium.components.sync.protocol.PasswordSpecificsData;

/** Tests that backend consumer bridge calls for operation callbacks reach native backend. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordStoreAndroidBackendReceiverBridgeTest {

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
    private static final long sFakeNativePointer = 4;
    private static final int sTestJobId = 1337;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock
    private PasswordStoreAndroidBackendReceiverBridgeImpl.Natives mBackendReceiverBridgeJniMock;

    private PasswordStoreAndroidBackendReceiverBridgeImpl mBackendReceiverBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(
                PasswordStoreAndroidBackendReceiverBridgeImplJni.TEST_HOOKS,
                mBackendReceiverBridgeJniMock);
        mBackendReceiverBridge =
                new PasswordStoreAndroidBackendReceiverBridgeImpl(sFakeNativePointer);
    }

    @Test
    public void testOnCompleteWithLoginsCallsBridge() {
        final byte[] kExpectedList = sTestLogins.build().toByteArray();

        mBackendReceiverBridge.onCompleteWithLogins(sTestJobId, kExpectedList);
        verify(mBackendReceiverBridgeJniMock)
                .onCompleteWithLogins(sFakeNativePointer, sTestJobId, kExpectedList);
    }

    @Test
    public void testOnCompleteWithBrandedLoginsCallsBridge() {
        PasswordWithUiInfo password =
                PasswordWithUiInfo.newBuilder().setPasswordData(sTestPwdWithLocalData).build();

        ListPasswordsWithUiInfoResult.Builder passwordsResult =
                ListPasswordsWithUiInfoResult.newBuilder().addPasswordsWithUiInfo(password);
        final byte[] kExpectedList = passwordsResult.build().toByteArray();

        mBackendReceiverBridge.onCompleteWithBrandedLogins(sTestJobId, kExpectedList);
        verify(mBackendReceiverBridgeJniMock)
                .onCompleteWithBrandedLogins(sFakeNativePointer, sTestJobId, kExpectedList);
    }

    @Test
    public void testOnCompleteWithAffiliatedLoginsCallsBridge() {
        AffiliatedPassword affiliatedPassword =
                AffiliatedPassword.newBuilder().setPasswordData(sTestPwdWithLocalData).build();

        ListAffiliatedPasswordsResult.Builder affiliatedPasswordsResult =
                ListAffiliatedPasswordsResult.newBuilder()
                        .addAffiliatedPasswords(affiliatedPassword);
        final byte[] kExpectedList = affiliatedPasswordsResult.build().toByteArray();

        mBackendReceiverBridge.onCompleteWithAffiliatedLogins(sTestJobId, kExpectedList);
        verify(mBackendReceiverBridgeJniMock)
                .onCompleteWithAffiliatedLogins(sFakeNativePointer, sTestJobId, kExpectedList);
    }

    @Test
    public void testOnApiExceptionCallsBridgeOnError() {
        Exception kExpectedException =
                new ApiException(
                        new Status(
                                new ConnectionResult(ConnectionResult.API_UNAVAILABLE),
                                "Test API error"));
        mBackendReceiverBridge.handleAndroidBackendException(sTestJobId, kExpectedException);
        verify(mBackendReceiverBridgeJniMock)
                .onError(
                        sFakeNativePointer,
                        sTestJobId,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        CommonStatusCodes.API_NOT_CONNECTED,
                        true,
                        ConnectionResult.API_UNAVAILABLE);
    }

    @Test
    public void testOnBackendExceptionCallsBridgeOnError() {
        Exception kExpectedException =
                new PasswordStoreAndroidBackend.BackendException(
                        "Test backend error", AndroidBackendErrorType.NO_ACCOUNT);
        mBackendReceiverBridge.handleAndroidBackendException(sTestJobId, kExpectedException);
        verify(mBackendReceiverBridgeJniMock)
                .onError(
                        sFakeNativePointer,
                        sTestJobId,
                        AndroidBackendErrorType.NO_ACCOUNT,
                        0,
                        false,
                        -1);
    }

    @Test
    public void testOnUnknownExceptionCallsBridgeOnError() {
        Exception kExpectedException = new Exception("Test error");
        mBackendReceiverBridge.handleAndroidBackendException(sTestJobId, kExpectedException);
        verify(mBackendReceiverBridgeJniMock)
                .onError(
                        sFakeNativePointer,
                        sTestJobId,
                        AndroidBackendErrorType.UNCATEGORIZED,
                        0,
                        false,
                        -1);
    }

    @Test
    public void testOnLoginChangedCallsBridge() {
        mBackendReceiverBridge.onLoginChanged(sTestJobId);
        verify(mBackendReceiverBridgeJniMock).onLoginChanged(sFakeNativePointer, sTestJobId);
    }
}
