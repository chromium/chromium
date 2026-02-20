// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.os.Looper;

import org.robolectric.Shadows;
import org.robolectric.android.util.concurrent.PausedExecutorService;

import org.chromium.base.task.PostTask;

/** Chrome utilities for Robolectric tests. */
public class RobolectricUtil {
    private RobolectricUtil() {}

    /** Runs a single queued background task */
    public static void runOneBackgroundTask() {
        if (!BaseRobolectricTestRule.sPausedExecutor.runNext()) {
            throw new AssertionError("No pending task");
        }
    }

    /**
     * Runs all queued background and UI non-delayed tasks and waits for them to finish.
     *
     * <p>Warning: This will deadlock if a background tasks blocks on the UI thread.
     *
     * @return How many background tasks were run.
     */
    public static int runAllBackgroundAndUi() {
        PausedExecutorService executor = BaseRobolectricTestRule.sPausedExecutor;
        assert executor != null;

        int taskCount = 0;
        for (int i = 0; i < 100; ++i) {
            Shadows.shadowOf(Looper.getMainLooper()).idle();
            if (!executor.hasQueuedTasks()) {
                return taskCount;
            }
            taskCount += executor.runAll();
        }
        throw new AssertionError("Infinite loop of background->foreground->background jobs");
    }

    /**
     * Runs all queued background and UI tasks, delayed and non-delayed, and waits for them to
     * finish.
     *
     * <p>Warning: This will deadlock if a background or a delayed task blocks on the UI thread.
     *
     * @return How many background or delayed tasks were run.
     */
    public static int runAllBackgroundAndUiIncludingDelayed() {
        PausedExecutorService executor = BaseRobolectricTestRule.sPausedExecutor;
        assert executor != null;

        int taskCount = 0;
        for (int i = 0; i < 100; ++i) {
            Shadows.shadowOf(Looper.getMainLooper()).runToEndOfTasks();
            if (!executor.hasQueuedTasks()) {
                return taskCount;
            }
            taskCount += executor.runAll();
        }
        throw new AssertionError("Infinite loop of background/foreground/delayed jobs");
    }

    /** Causes PostTask tasks to be run on background threads (like in a non-test environment). */
    public static void uninstallPausedExecutorService() {
        PausedExecutorService executor = BaseRobolectricTestRule.sPausedExecutor;
        assert executor != null;

        // Should uninstall before any tasks are posted.
        assert !executor.hasQueuedTasks();

        PostTask.setPrenativeThreadPoolExecutorForTesting(null);
        PostTask.setPrenativeThreadPoolDelayedExecutorForTesting(null);
    }
}
