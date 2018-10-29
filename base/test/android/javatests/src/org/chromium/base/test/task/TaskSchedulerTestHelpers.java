// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.task;

/** Helpers that allow base::TaskScheduler to be initialized or shutdown for testing. */
public class TaskSchedulerTestHelpers {
    /**
     * Initializes base::TaskScheduler with default params.
     */
    public static void enableTaskSchedulerExecutionForTesting() {
        nativeEnableTaskSchedulerExecutionForTesting();
    }

    /**
     * Shuts down base::TaskScheduler.
     */
    public static void disableTaskSchedulerExecutionForTesting() {
        nativeDisableTaskSchedulerExecutionForTesting();
    }

    private static native void nativeEnableTaskSchedulerExecutionForTesting();
    private static native void nativeDisableTaskSchedulerExecutionForTesting();
}
