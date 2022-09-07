// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;
import android.os.Process;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeUnchecked;
import org.chromium.base.annotations.JNINamespace;

import java.util.concurrent.atomic.AtomicBoolean;

@JNINamespace("base::android")
class JavaHandlerThreadHelpers {
    private static class TestException extends Exception {}

    // This is executed as part of base_unittests. This tests that JavaHandlerThread can be used
    // by itself without attaching to its native peer.
    @CalledByNative
    private static JavaHandlerThread testAndGetJavaHandlerThread() {
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        final Object lock = new Object();
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notifyAll();
                }
            }
        };

        JavaHandlerThread thread =
                new JavaHandlerThread("base_unittests_java", Process.THREAD_PRIORITY_DEFAULT);
        thread.maybeStart();

        Handler handler = new Handler(thread.getLooper());
        handler.post(runnable);
        synchronized (lock) {
            while (!taskExecuted.get()) {
                try {
                    lock.wait();
                } catch (InterruptedException e) {
                    // ignore interrupts
                }
            }
        }

        return thread;
    }

    @CalledByNativeUnchecked
    private static void throwException() throws TestException {
        throw new TestException();
    }

    @CalledByNative
    private static boolean isExceptionTestException(Throwable exception) {
        if (exception == null) return false;
        return exception instanceof TestException;
    }
}
