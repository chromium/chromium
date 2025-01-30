// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("base::android")
final class JniCallbackUtils {

    static void runNativeCallback(JniOnceCallback callback, Object result) {
        assert callback.mNativePointer != 0;
        JniCallbackUtilsJni.get().onResult(callback.mNativePointer, false, result);
        callback.mNativePointer = 0;
    }

    static void runNativeCallback(JniRepeatingCallback callback, Object result) {
        assert callback.mNativePointer != 0;
        JniCallbackUtilsJni.get().onResult(callback.mNativePointer, true, result);
    }

    static void destroyNativeCallback(JniOnceCallback callback) {
        assert callback.mNativePointer != 0;
        JniCallbackUtilsJni.get().destroy(callback.mNativePointer, false);
        callback.mNativePointer = 0;
    }

    static void destroyNativeCallback(JniRepeatingCallback callback) {
        assert callback.mNativePointer != 0;
        JniCallbackUtilsJni.get().destroy(callback.mNativePointer, true);
        callback.mNativePointer = 0;
    }

    @NativeMethods
    interface Natives {
        void onResult(long callbackPtr, boolean isRepeating, Object result);

        void destroy(long callbackPtr, boolean isRepeating);
    }
}
