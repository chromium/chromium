// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

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
     * Each operation sent to the passwords API will be assigned a TaskId. The native side uses
     * this ID to map an API response to the task that invoked it.
     */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface TaskId {}

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
    void getAllLogins(@TaskId int taskId) {
        mBackend.getAllLogins(
                passwords
                -> {
                    if (mNativeBackendBridge == 0) return;
                    PasswordStoreAndroidBackendBridgeImplJni.get().onCompleteWithLogins(
                            mNativeBackendBridge, taskId, passwords);
                },
                exception
                -> {
                        // TODO(crbug.com/1229654): Clear failed tasks and record failures.
                });
    }

    @CalledByNative
    private void destroy() {
        mNativeBackendBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativePasswordStoreAndroidBackendBridgeImpl,
                @TaskId int taskId, byte[] passwords);
    }
}
