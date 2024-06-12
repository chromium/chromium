// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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

/**
 * Tests that bridge calls as invoked by the password sync controller delegate reach the delegate
 * and return correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordSyncControllerDelegateBridgeTest {

    private static final long sFakeNativePointer = 4;

    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private static final Exception EXPECTED_EXCEPTION = new Exception("Sample failure");
    private static final int EXPECTED_API_ERROR_CODE = CommonStatusCodes.INTERNAL_ERROR;
    private static final Exception EXPECTED_API_EXCEPTION =
            new ApiException(new Status(EXPECTED_API_ERROR_CODE, ""));

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private PasswordSyncControllerDelegateBridgeImpl.Natives mBridgeJniMock;
    @Mock private PasswordSyncControllerDelegate mDelegateMock;

    private PasswordSyncControllerDelegateBridgeImpl mDelegateBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PasswordSyncControllerDelegateBridgeImplJni.TEST_HOOKS, mBridgeJniMock);
        mDelegateBridge =
                new PasswordSyncControllerDelegateBridgeImpl(sFakeNativePointer, mDelegateMock);
    }

    @Test
    public void testNotifyCredentialManagerWhenSyncingCallsBridgeOnSuccess() {
        mDelegateBridge.notifyCredentialManagerWhenSyncing(TEST_EMAIL_ADDRESS);
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);
        verify(mDelegateMock)
                .notifyCredentialManagerWhenSyncing(
                        eq(TEST_EMAIL_ADDRESS), successCallback.capture(), any());

        assertNotNull(successCallback.getValue());
        successCallback.getValue().run();
        verify(mBridgeJniMock).onCredentialManagerNotified(sFakeNativePointer);
    }

    @Test
    public void testNotifyCredentialManagerWhenSyncingCallsBridgeOnFailure() {
        mDelegateBridge.notifyCredentialManagerWhenSyncing(TEST_EMAIL_ADDRESS);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mDelegateMock)
                .notifyCredentialManagerWhenSyncing(
                        eq(TEST_EMAIL_ADDRESS), any(), failureCallback.capture());

        assertNotNull(failureCallback.getValue());
        failureCallback.getValue().onResult(EXPECTED_EXCEPTION);
        verify(mBridgeJniMock)
                .onCredentialManagerError(
                        sFakeNativePointer, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }

    @Test
    public void testNotifyCredentialManagerWhenSyncingCallsBridgeOnAPIError() {
        mDelegateBridge.notifyCredentialManagerWhenSyncing(TEST_EMAIL_ADDRESS);
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);
        verify(mDelegateMock)
                .notifyCredentialManagerWhenSyncing(
                        eq(TEST_EMAIL_ADDRESS), any(), failureCallback.capture());

        assertNotNull(failureCallback.getValue());
        failureCallback.getValue().onResult(EXPECTED_API_EXCEPTION);
        verify(mBridgeJniMock)
                .onCredentialManagerError(
                        sFakeNativePointer,
                        AndroidBackendErrorType.EXTERNAL_ERROR,
                        EXPECTED_API_ERROR_CODE);
    }

    @Test
    public void testNotifyCredentialManagerWhenNotSyncingCallsBridgeOnSuccess() {
        // Ensure the delegate is called with a valid success callback.
        mDelegateBridge.notifyCredentialManagerWhenNotSyncing();
        ArgumentCaptor<Runnable> successCallback = ArgumentCaptor.forClass(Runnable.class);

        verify(mDelegateMock)
                .notifyCredentialManagerWhenNotSyncing(successCallback.capture(), any());
        assertNotNull(successCallback.getValue());

        successCallback.getValue().run();
        verify(mBridgeJniMock).onCredentialManagerNotified(sFakeNativePointer);
    }

    @Test
    public void testNotifyCredentialManagerWhenNotSyncingCallsBridgeOnFailure() {
        // Ensure the delegate is called with a valid failure callback.
        mDelegateBridge.notifyCredentialManagerWhenNotSyncing();
        ArgumentCaptor<Callback<Exception>> failureCallback =
                ArgumentCaptor.forClass(Callback.class);

        verify(mDelegateMock)
                .notifyCredentialManagerWhenNotSyncing(any(), failureCallback.capture());
        assertNotNull(failureCallback.getValue());

        failureCallback.getValue().onResult(EXPECTED_EXCEPTION);
        verify(mBridgeJniMock)
                .onCredentialManagerError(
                        sFakeNativePointer, AndroidBackendErrorType.UNCATEGORIZED, 0);
    }
}
