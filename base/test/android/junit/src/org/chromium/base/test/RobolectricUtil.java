// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.os.Looper;

import org.robolectric.Shadows;
import org.robolectric.android.util.concurrent.PausedExecutorService;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.JavaUtils;
import org.chromium.base.task.PostTask;

import java.time.Duration;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

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
        ShadowLooper mainLooper = Shadows.shadowOf(Looper.getMainLooper());
        for (int i = 0; i < 100; ++i) {
            mainLooper.idle();
            if (!executor.hasQueuedTasks()) {
                return taskCount;
            }
            try {
                taskCount += executor.runAll();
            } catch (Throwable e) {
                e = e.getCause();
                if (e instanceof InterruptedException) {
                    // Use multi-line string to make the hint more noticeable, since our test runner
                    // make it show as a suppressed exception (not at the top of the backtrace).
                    throw new RuntimeException(
                            """

                            Timed out while in runAllBackgroundAndUi().
                            This is often due to a background task blocking on a UI task.
                            Use runAllBackgroundAndUiAllowBlocking() instead.
                            """,
                            e);
                }
                JavaUtils.throwUnchecked(e);
            }
        }
        throw new AssertionError("Infinite loop of background->foreground->background jobs");
    }

    /**
     * Runs all queued background and UI non-delayed tasks and waits for them to finish.
     *
     * <p>Will not deadlock if background thread blocks on UI thread.
     */
    public static void runAllBackgroundAndUiAllowBlocking() {
        PausedExecutorService executor = BaseRobolectricTestRule.sPausedExecutor;
        assert executor != null;

        AtomicBoolean done = new AtomicBoolean(false);
        AtomicReference<Throwable> exception = new AtomicReference<>();
        ShadowLooper mainLooper = Shadows.shadowOf(Looper.getMainLooper());
        for (int i = 0; i < 100; ++i) {
            mainLooper.idle();
            if (!executor.hasQueuedTasks()) {
                return;
            }
            new Thread(
                            () -> {
                                try {
                                    executor.runAll();
                                } catch (Throwable t) {
                                    exception.set(t);
                                } finally {
                                    done.set(true);
                                }
                            },
                            "runAllBackgroundAndUiAllowBlocking")
                    .start();

            // Busy loop executing all UI tasks that get posted.
            while (!done.get()) {
                try {
                    Thread.sleep(1);
                } catch (InterruptedException e) {
                    JavaUtils.throwUnchecked(e);
                }
                mainLooper.idle();
            }
            Throwable backgroundException = exception.get();
            if (backgroundException != null) {
                throw new RuntimeException(backgroundException);
            }
            done.set(false);
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

        ShadowLooper mainLooper = Shadows.shadowOf(Looper.getMainLooper());
        int taskCount = 0;
        for (int i = 0; i < 100; ++i) {
            mainLooper.runToEndOfTasks();
            taskCount += executor.runAll();
            if (mainLooper.getLastScheduledTaskTime().equals(Duration.ZERO)) {
                return taskCount;
            }
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
