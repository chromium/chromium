// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.one_time_passwords;

import com.google.android.gms.common.api.ApiException;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * Java-counterpart of the native AndroidSmsOtpFetchReceiverBridge. It's part of the OTP value
 * fetching backend that forwards operation callbacks to the native password manager.
 */
@NullMarked
class AndroidSmsOtpFetchReceiverBridge {
    private long mNativeReceiverBridge;

    AndroidSmsOtpFetchReceiverBridge(long nativeReceiverBridge) {
        mNativeReceiverBridge = nativeReceiverBridge;
    }

    @CalledByNative
    static AndroidSmsOtpFetchReceiverBridge create(long nativeReceiverBridge) {
        return new AndroidSmsOtpFetchReceiverBridge(nativeReceiverBridge);
    }

    void onOtpValueRetrieved(String otpValue) {
        if (mNativeReceiverBridge == 0) return;
        AndroidSmsOtpFetchReceiverBridgeJni.get()
                .onOtpValueRetrieved(mNativeReceiverBridge, otpValue);
    }

    void onOtpValueRetrievalError(ApiException exception) {
        if (mNativeReceiverBridge == 0) return;

        AndroidSmsOtpFetchReceiverBridgeJni.get()
                .onOtpValueRetrievalError(mNativeReceiverBridge, exception.getStatusCode());
    }

    @CalledByNative
    private void destroy() {
        mNativeReceiverBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onOtpValueRetrieved(long nativeAndroidSmsOtpFetchReceiverBridge, String otpValue);

        void onOtpValueRetrievalError(
                long nativeAndroidSmsOtpFetchReceiverBridge, int apiErrorCode);
    }
}
