// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Java-counterpart of the native PasswordStoreAndroidBackendReceiverBridgeImpl. It's part of the
 * password store backend that forwards operation callbacks to the native password manager.
 */
@JNINamespace("password_manager")
class PasswordStoreAndroidBackendReceiverBridgeImpl {
    /**
     * Each operation sent to the passwords API will be assigned a JobId. The native side uses
     * this ID to map an API response to the job that invoked it.
     */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface JobId {}

    private long mNativeBackendReceiverBridge;

    PasswordStoreAndroidBackendReceiverBridgeImpl(long nativeBackendReceiverBridge) {
        mNativeBackendReceiverBridge = nativeBackendReceiverBridge;
    }

    @CalledByNative
    static PasswordStoreAndroidBackendReceiverBridgeImpl create(long nativeBackendReceiverBridge) {
        return new PasswordStoreAndroidBackendReceiverBridgeImpl(nativeBackendReceiverBridge);
    }

    void handleAndroidBackendException(@JobId int jobId, Exception exception) {
        if (mNativeBackendReceiverBridge == 0) return;

        @AndroidBackendErrorType
        int error = PasswordManagerAndroidBackendUtil.getBackendError(exception);
        int apiErrorCode = PasswordManagerAndroidBackendUtil.getApiErrorCode(exception);
        Integer connectionResultCode =
                PasswordManagerAndroidBackendUtil.getConnectionResultCode(exception);

        PasswordStoreAndroidBackendReceiverBridgeImplJni.get()
                .onError(
                        mNativeBackendReceiverBridge,
                        jobId,
                        error,
                        apiErrorCode,
                        connectionResultCode != null,
                        connectionResultCode == null ? -1 : connectionResultCode.intValue());
    }

    void onCompleteWithLogins(@JobId int jobId, byte[] passwords) {
        if (mNativeBackendReceiverBridge == 0) return;
        PasswordStoreAndroidBackendReceiverBridgeImplJni.get()
                .onCompleteWithLogins(mNativeBackendReceiverBridge, jobId, passwords);
    }

    void onCompleteWithBrandedLogins(@JobId int jobId, byte[] passwords) {
        if (mNativeBackendReceiverBridge == 0) return;
        PasswordStoreAndroidBackendReceiverBridgeImplJni.get()
                .onCompleteWithBrandedLogins(mNativeBackendReceiverBridge, jobId, passwords);
    }

    void onCompleteWithAffiliatedLogins(@JobId int jobId, byte[] passwords) {
        if (mNativeBackendReceiverBridge == 0) return;
        PasswordStoreAndroidBackendReceiverBridgeImplJni.get()
                .onCompleteWithAffiliatedLogins(mNativeBackendReceiverBridge, jobId, passwords);
    }

    void onLoginChanged(@JobId int jobId) {
        if (mNativeBackendReceiverBridge == 0) return;
        PasswordStoreAndroidBackendReceiverBridgeImplJni.get()
                .onLoginChanged(mNativeBackendReceiverBridge, jobId);
    }

    void onError(
            @JobId int jobId,
            int errorType,
            int apiErrorCode,
            boolean hasConnectionResult,
            int connectionResultStatusCode) {
        if (mNativeBackendReceiverBridge == 0) return;
        PasswordStoreAndroidBackendReceiverBridgeImplJni.get()
                .onError(
                        mNativeBackendReceiverBridge,
                        jobId,
                        errorType,
                        apiErrorCode,
                        hasConnectionResult,
                        connectionResultStatusCode);
    }

    @CalledByNative
    private void destroy() {
        mNativeBackendReceiverBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(
                long nativePasswordStoreAndroidBackendReceiverBridgeImpl,
                @JobId int jobId,
                byte[] passwords);

        void onCompleteWithBrandedLogins(
                long nativePasswordStoreAndroidBackendReceiverBridgeImpl,
                @JobId int jobId,
                byte[] passwords);

        void onCompleteWithAffiliatedLogins(
                long nativePasswordStoreAndroidBackendReceiverBridgeImpl,
                @JobId int jobId,
                byte[] passwords);

        void onLoginChanged(
                long nativePasswordStoreAndroidBackendReceiverBridgeImpl, @JobId int jobId);

        void onError(
                long nativePasswordStoreAndroidBackendReceiverBridgeImpl,
                @JobId int jobId,
                int errorType,
                int apiErrorCode,
                boolean hasConnectionResult,
                int connectionResultStatusCode);
    }
}
