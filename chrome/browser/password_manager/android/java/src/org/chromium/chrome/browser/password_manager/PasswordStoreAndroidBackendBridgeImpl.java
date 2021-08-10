// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.sync.protocol.ListPasswordsResult;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Java-counterpart of the native PasswordStoreAndroidBackendBridgeImpl. It's part of the password
 * store backend that forwards password store operations to Google Mobile Services.
 */
class PasswordStoreAndroidBackendBridgeImpl {
    /**
     * Each operation sent to the passwords API will be assigned a TaskId. The native side uses
     * this ID to map an API response to the task that invoked it.
     */
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface TaskId {}

    private long mNativeBackendBridge;

    private PasswordStoreAndroidBackendBridgeImpl(long nativeBackendBridge) {
        mNativeBackendBridge = nativeBackendBridge;
    }

    @CalledByNative
    private static PasswordStoreAndroidBackendBridgeImpl create(long nativeBackendBridge) {
        return new PasswordStoreAndroidBackendBridgeImpl(nativeBackendBridge);
    }

    @CalledByNative
    private void getAllLogins(@TaskId int taskId) {
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
            if (mNativeBackendBridge == 0) return;
            // TODO(crbug.com/1229654):Implement.
            PasswordStoreAndroidBackendBridgeImplJni.get().onCompleteWithLogins(
                    mNativeBackendBridge, taskId, ListPasswordsResult.getDefaultInstance());
        });
    }

    @CalledByNative
    private void destroy() {
        mNativeBackendBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onCompleteWithLogins(long nativePasswordStoreAndroidBackendBridgeImpl,
                @TaskId int taskId, ListPasswordsResult passwords);
    }
}
