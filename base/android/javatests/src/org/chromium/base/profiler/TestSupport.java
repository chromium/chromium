// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.profiler;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** Helper to run code through JNI layer to test JNI unwinding. */
@JNINamespace("base")
public final class TestSupport {
    @CalledByNative
    public static void callWithJavaFunction(long context) {
        TestSupportJni.get().invokeCallbackFunction(context);
    }

    @NativeMethods
    interface Natives {
        void invokeCallbackFunction(long context);
    }
}
