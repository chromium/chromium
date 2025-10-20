// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayDeque;

import javax.annotation.concurrent.GuardedBy;

/**
 * Allows chaining multiple tasks on arbitrary threads, with the next task posted when one
 * completes.
 *
 * <p>How this differs from SequencedTaskRunner: Deferred posting of subsequent tasks allows more
 * time for Android framework tasks to run (e.g. input events). As such, this class really only
 * makes sense when submitting tasks to the UI thread.
 *
 * <p>Threading: - This class is threadsafe and all methods may be called from any thread. - Tasks
 * may run with arbitrary TaskTraits, unless tasks are coalesced, in which case all tasks must run
 * on the same thread.
 */
@NullMarked
public class ChainedTasks {
    private final ArrayDeque<ChainedTask> mTasks = new ArrayDeque<>();

    @GuardedBy("mTasks")
    private boolean mFinalized;

    private volatile boolean mCanceled;
    private final int mIterationIdForTesting = PostTask.sTestIterationForTesting;

    private void runAndPost() {
        if (mIterationIdForTesting != PostTask.sTestIterationForTesting) {
            cancel();
        }
        if (mCanceled) return;

        ChainedTask task = mTasks.pop();
        task.run();
        if (!mTasks.isEmpty()) {
            ChainedTask nextTask = mTasks.peek();
            PostTask.postTask(nextTask.mTaskTraits, this::runAndPost, nextTask.mLocation);
        }
    }

    private static class ChainedTask implements Runnable {
        private final @TaskTraits int mTaskTraits;
        private final Runnable mRunnable;
        private final @Nullable Location mLocation;

        ChainedTask(@TaskTraits int traits, Runnable runnable, @Nullable Location location) {
            mTaskTraits = traits;
            mRunnable = runnable;
            mLocation = location;
        }

        @Override
        public void run() {
            // TODO(anandrv): Remove trace event once location rewriting is enabled by default
            try (TraceEvent e =
                    TraceEvent.scoped(
                            "ChainedTask.run", (mLocation != null) ? mLocation.toString() : null)) {
                mRunnable.run();
            }
        }
    }

    /**
     * Adds a task to the list of tasks to run. Cannot be called once {@link start()} has been
     * called.
     */
    public void add(@TaskTraits int traits, Runnable task) {
        add(traits, task, null);
    }

    /**
     * Do not call this method directly unless forwarding a location object. Use {@link #add(int,
     * Runnable)} instead.
     *
     * <p>Overload of {@link #add(int, Runnable)} for the Java location rewriter.
     */
    public void add(@TaskTraits int traits, Runnable task, @Nullable Location location) {
        assert mIterationIdForTesting == PostTask.sTestIterationForTesting;
        if (PostTask.ENABLE_TASK_ORIGINS) {
            task = PostTask.populateTaskOrigin(new TaskOriginException(), task);
        }
        synchronized (mTasks) {
            assert !mFinalized : "Must not call add() after start()";
            mTasks.add(new ChainedTask(traits, task, location));
        }
    }

    /** Cancels the remaining tasks. */
    public void cancel() {
        synchronized (mTasks) {
            mFinalized = true;
            mCanceled = true;
        }
    }

    /**
     * Posts or runs all the tasks, one by one.
     *
     * @param coalesceTasks if false, posts the tasks. Otherwise run them in a single task. If
     * called on the thread matching the TaskTraits, will block and run all tasks synchronously.
     */
    public void start(final boolean coalesceTasks) {
        synchronized (mTasks) {
            assert !mFinalized : "Cannot call start() several times";
            mFinalized = true;
        }
        if (mTasks.isEmpty()) return;

        ChainedTask nextTask = mTasks.peek();
        if (coalesceTasks) {
            PostTask.runOrPostTask(
                    nextTask.mTaskTraits,
                    () -> {
                        for (ChainedTask task : mTasks) {
                            assert PostTask.canRunTaskImmediately(task.mTaskTraits);
                            task.run();
                            if (mCanceled) return;
                        }
                    });
        } else {
            PostTask.postTask(nextTask.mTaskTraits, this::runAndPost, nextTask.mLocation);
        }
    }
}
