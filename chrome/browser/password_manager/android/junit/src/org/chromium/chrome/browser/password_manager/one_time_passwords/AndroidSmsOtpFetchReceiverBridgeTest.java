// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.one_time_passwords;

import static org.mockito.Mockito.verify;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class AndroidSmsOtpFetchReceiverBridgeTest {

    private static final long S_FAKE_NATIVE_POINTER = 7;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AndroidSmsOtpFetchReceiverBridge.Natives mReceiverBridgeJniMock;

    private AndroidSmsOtpFetchReceiverBridge mReceiverBridge;

    @Before
    public void setUp() {
        AndroidSmsOtpFetchReceiverBridgeJni.setInstanceForTesting(mReceiverBridgeJniMock);
        mReceiverBridge = new AndroidSmsOtpFetchReceiverBridge(S_FAKE_NATIVE_POINTER);
    }

    @Test
    public void testOnOtpValueRetrieved() {
        mReceiverBridge.onOtpValueRetrieved("123456");
        verify(mReceiverBridgeJniMock).onOtpValueRetrieved(S_FAKE_NATIVE_POINTER, "123456");
    }

    @Test
    public void testOnOtpValueRetrievalError() {
        ApiException receivedException = new ApiException(new Status(CommonStatusCodes.TIMEOUT));
        mReceiverBridge.onOtpValueRetrievalError(receivedException);
        verify(mReceiverBridgeJniMock)
                .onOtpValueRetrievalError(S_FAKE_NATIVE_POINTER, receivedException.getStatusCode());
    }
}
