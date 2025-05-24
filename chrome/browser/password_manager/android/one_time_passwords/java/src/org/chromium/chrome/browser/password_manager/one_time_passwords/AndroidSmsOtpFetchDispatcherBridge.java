// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.one_time_passwords;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Java-counterpart of the native AndroidSmsOtpFetchDispatcherBridge. It's part of the OTP value
 * fetching backend that forwards retrieval requests to a downstream implementation.
 */
@NullMarked
class AndroidSmsOtpFetchDispatcherBridge {
    private final AndroidSmsOtpFetchReceiverBridge mReceiverBridge;
    private final AndroidSmsOtpFetcher mOtpFetcher;

    AndroidSmsOtpFetchDispatcherBridge(
            AndroidSmsOtpFetchReceiverBridge receiverBridge, AndroidSmsOtpFetcher otpFetcher) {
        mReceiverBridge = receiverBridge;
        mOtpFetcher = otpFetcher;
    }

    @CalledByNative
    static @Nullable AndroidSmsOtpFetchDispatcherBridge create(
            AndroidSmsOtpFetchReceiverBridge receiverBridge) {
        AndroidSmsOtpFetcher otpFetcher =
                AndroidSmsOtpFetcherFactory.getInstance().createSmsOtpFetcher();
        if (otpFetcher == null) {
            return null;
        }
        return new AndroidSmsOtpFetchDispatcherBridge(receiverBridge, otpFetcher);
    }

    @CalledByNative
    void retrieveSmsOtp() {
        mOtpFetcher.retrieveSmsOtp(
                otpValue -> mReceiverBridge.onOtpValueRetrieved(otpValue),
                exception -> mReceiverBridge.onOtpValueRetrievalError(exception));
    }
}
