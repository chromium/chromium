// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Java-counterpart of the native PasswordStoreAndroidBackendConsumerBridgeImpl. It's part of the
 * password store backend that forwards operation callbacks to the native password manager.
 */
@JNINamespace("password_manager")
class PasswordStoreAndroidBackendConsumerBridgeImpl {
    /**
     * Each operation sent to the passwords API will be assigned a JobId. The native side uses
     * this ID to map an API response to the job that invoked it.
     */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface JobId {}

    private long mNativeConsumerBridge;

    PasswordStoreAndroidBackendConsumerBridgeImpl(long nativeConsumerBridge) {
        mNativeConsumerBridge = nativeConsumerBridge;
    }

    @CalledByNative
    static PasswordStoreAndroidBackendConsumerBridgeImpl create(long nativeConsumerBridge) {
        return new PasswordStoreAndroidBackendConsumerBridgeImpl(nativeConsumerBridge);
    }

    void handleAndroidBackendException(@JobId int jobId, Exception exception) {
        if (mNativeConsumerBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
        Integer connectionResultCode =
                PasswordManagerAndroidBackendUtil.getConnectionResultCode(exception);

        PasswordStoreAndroidBackendConsumerBridgeImplJni.get().onError(mNativeConsumerBridge, jobId,
                error, apiErrorCode, connectionResultCode != null,
                connectionResultCode == null ? -1 : connectionResultCode.intValue());
    }

    void onCompleteWithLogins(@JobId int jobId, byte[] passwords) {
        if (mNativeConsumerBridge == 0) return;
        PasswordStoreAndroidBackendConsumerBridgeImplJni.get().onCompleteWithLogins(
                mNativeConsumerBridge, jobId, passwords);
    }

    void onLoginChanged(@JobId int jobId) {
        if (mNativeConsumerBridge == 0) return;
        PasswordStoreAndroidBackendConsumerBridgeImplJni.get().onLoginChanged(
                mNativeConsumerBridge, jobId);
    }

    void onError(@JobId int jobId, int errorType, int apiErrorCode, boolean hasConnectionResult,
            int connectionResultStatusCode) {
        if (mNativeConsumerBridge == 0) return;
        PasswordStoreAndroidBackendConsumerBridgeImplJni.get().onError(mNativeConsumerBridge, jobId,
                errorType, apiErrorCode, hasConnectionResult, connectionResultStatusCode);
    }

    @CalledByNative
    private void destroy() {
        mNativeConsumerBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativePasswordStoreAndroidBackendConsumerBridgeImpl,
                @JobId int jobId, byte[] passwords);
        void onLoginChanged(
                long nativePasswordStoreAndroidBackendConsumerBridgeImpl, @JobId int jobId);
        void onError(long nativePasswordStoreAndroidBackendConsumerBridgeImpl, @JobId int jobId,
                int errorType, int apiErrorCode, boolean hasConnectionResult,
                int connectionResultStatusCode);
    }
}
