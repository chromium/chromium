// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeUnchecked;

final class JniAndroidTestUtils {
    private JniAndroidTestUtils() {}

    private static Thread.UncaughtExceptionHandler sOldHandler;

    @CalledByNative
    private static void throwRuntimeException() {
        throw new RuntimeException("test exception");
    }

    @CalledByNativeUnchecked
    private static void throwRuntimeExceptionUnchecked() {
        throw new RuntimeException("test exception");
    }

    @CalledByNative
    private static void throwOutOfMemoryError() {
        throw new OutOfMemoryError("test exception");
    }

    @CalledByNative
    private static void setSimulateOomInSanitizedStacktrace(boolean value) {
        JniAndroid.sSimulateOomInSanitizedStacktraceForTesting = value;
    }

    @CalledByNative
    private static void setGlobalExceptionHandlerAsNoOp() {
        assert sOldHandler == null;
        sOldHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, exception) -> {});
    }

    @CalledByNative
    private static void setGlobalExceptionHandlerToThrow() {
        assert sOldHandler == null;
        sOldHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(
                (thread, exception) -> {
                    throw new IllegalStateException();
                });
    }

    @CalledByNative
    private static void setGlobalExceptionHandlerToThrowOom() {
        assert sOldHandler == null;
        sOldHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(
                (thread, exception) -> {
                    throw new OutOfMemoryError();
                });
    }

    @CalledByNative
    private static void restoreGlobalExceptionHandler() {
        if (sOldHandler != null) {
            Thread.setDefaultUncaughtExceptionHandler(sOldHandler);
            sOldHandler = null;
        }
    }
}
