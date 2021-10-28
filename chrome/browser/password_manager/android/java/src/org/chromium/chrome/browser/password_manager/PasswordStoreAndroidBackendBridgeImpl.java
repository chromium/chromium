// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import com.google.android.gms.common.api.ApiException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Java-counterpart of the native PasswordStoreAndroidBackendBridgeImpl. It's part of the password
 * store backend that forwards password store operations to a downstream implementation.
 */
class PasswordStoreAndroidBackendBridgeImpl {
    /**
     * Each operation sent to the passwords API will be assigned a JobId. The native side uses
     * this ID to map an API response to the job that invoked it.
     */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface JobId {}

    private final PasswordStoreAndroidBackend mBackend;
    private long mNativeBackendBridge;

    PasswordStoreAndroidBackendBridgeImpl(
            long nativeBackendBridge, PasswordStoreAndroidBackend backend) {
        mNativeBackendBridge = nativeBackendBridge;
        mBackend = backend;
        assert mBackend != null;
    }

    @CalledByNative
    static PasswordStoreAndroidBackendBridgeImpl create(long nativeBackendBridge) {
        return new PasswordStoreAndroidBackendBridgeImpl(nativeBackendBridge,
                PasswordStoreAndroidBackendFactory.getInstance().createBackend());
    }

    @CalledByNative
    static boolean canCreateBackend() {
        return PasswordStoreAndroidBackendFactory.getInstance().canCreateBackend();
    }

    @CalledByNative
    void getAllLogins(@JobId int jobId) {
        mBackend.getAllLogins(passwords -> {
            if (mNativeBackendBridge == 0) return;
            PasswordStoreAndroidBackendBridgeImplJni.get().onCompleteWithLogins(
                    mNativeBackendBridge, jobId, passwords);
        }, exception -> handleAndroidBackendException(jobId, exception));
    }

    private void handleAndroidBackendException(@JobId int jobId, Exception exception) {
        if (mNativeBackendBridge == 0) return;

        @AndroidBackendErrorType
        int error = AndroidBackendErrorType.UNCATEGORIZED;

        if (exception instanceof PasswordStoreAndroidBackend.BackendException) {
            error = ((PasswordStoreAndroidBackend.BackendException) exception).errorCode;
        }

        if (exception instanceof ApiException) {
            error = AndroidBackendErrorType.EXTERNAL_ERROR;
        }

        PasswordStoreAndroidBackendBridgeImplJni.get().onError(mNativeBackendBridge, jobId, error);
    }

    @CalledByNative
    private void destroy() {
        mNativeBackendBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativePasswordStoreAndroidBackendBridgeImpl,
                @JobId int jobId, byte[] passwords);
        void onError(long nativePasswordStoreAndroidBackendBridgeImpl, @JobId int jobId, int error);
    }
}
