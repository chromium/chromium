// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.task;

import org.jni_zero.NativeMethods;

/** Helpers that allow base::ThreadPoolInstance to be initialized or shutdown for testing. */
public class ThreadPoolTestHelpers {
    /** Initializes base::ThreadPoolInstance with default params. */
    public static void enableThreadPoolExecutionForTesting() {
        ThreadPoolTestHelpersJni.get().enableThreadPoolExecutionForTesting();
    }

    /** Shuts down base::ThreadPoolInstance. */
    public static void disableThreadPoolExecutionForTesting() {
        ThreadPoolTestHelpersJni.get().disableThreadPoolExecutionForTesting();
    }

    @NativeMethods
    interface Natives {
        void enableThreadPoolExecutionForTesting();

        void disableThreadPoolExecutionForTesting();
    }
}
