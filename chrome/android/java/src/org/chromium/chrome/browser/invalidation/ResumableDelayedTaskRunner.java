// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Runner which can be paused. When the runner is paused, the execution of its
 * scheduled task is delayed till the runner is resumed. This runner could be
 * used as follows:
 *
 * <pre>
 * {@code
 *
 * ResumableDelayedTaskRunner runner = new ResumableDelayedTaskRunner();
 * runner.setRunnable(task, delayMs);
 * runner.resume();  // Starts the count down.
 * runner.pause();   // Pauses the count down.
 * runner.resume();  // Resumes the count down.
 * runner.cancel();  // Stops count down and clears the state.
 *
 * }
 * </pre>
 */
@NullMarked
public class ResumableDelayedTaskRunner {
    private final Handler mHandler = new Handler();
    private final Thread mThread = Thread.currentThread();

    /** Runnable which is added to the handler's message queue. */
    private @Nullable Runnable mHandlerRunnable;

    /** User provided task. */
    private @Nullable Runnable mRunnable;

    /** Time at which the task is scheduled. */
    private long mScheduledTime;

    /**
     * This requires to be called on a thread where the Looper has been
     * prepared.
     */
    public ResumableDelayedTaskRunner() {
        assert Looper.myLooper() != null
                : "ResumableDelayedTaskRunner can only be used on threads with a Looper";
    }

    /**
     * Sets the task to run. The task will be run after the delay once
     * {@link #resume()} is called. The previously scheduled task, if any, is
     * cancelled. The Runnable |r| shouldn't call setRunnable() itself.
     * @param r Task to run.
     * @param delayMs Delay in milliseconds after which to run the task.
     */
    public void setRunnable(Runnable r, long delayMs) {
        checkThread();
        cancel();
        mRunnable = r;
        mScheduledTime = SystemClock.elapsedRealtime() + delayMs;
    }

    /** Blocks the task from being run. */
    public void pause() {
        checkThread();
        if (mHandlerRunnable == null) {
            return;
        }

        mHandler.removeCallbacks(mHandlerRunnable);
        mHandlerRunnable = null;
    }

    /**
     * Unblocks the task from being run. If the task was scheduled for a time in the past, runs
     * the task. Does nothing if no task is scheduled.
     */
    public void resume() {
        checkThread();
        if (mRunnable == null || mHandlerRunnable != null) {
            return;
        }

        long delayMs = Math.max(mScheduledTime - SystemClock.elapsedRealtime(), 0);
        mHandlerRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        assumeNonNull(mRunnable);
                        mRunnable.run();
                        mRunnable = null;
                        mHandlerRunnable = null;
                    }
                };
        mHandler.postDelayed(mHandlerRunnable, delayMs);
    }

    /** Cancels the scheduled task, if any. */
    public void cancel() {
        checkThread();
        pause();
        mRunnable = null;
    }

    private void checkThread() {
        assert mThread == Thread.currentThread()
                : "ResumableDelayedTaskRunner must only be used on a single Thread.";
    }
}
