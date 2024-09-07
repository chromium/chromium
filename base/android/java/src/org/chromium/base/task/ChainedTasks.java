// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.util.Pair;

import org.chromium.base.TraceEvent;

import java.util.LinkedList;

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
public class ChainedTasks {
    private final LinkedList<Pair<Integer, Runnable>> mTasks = new LinkedList<>();

    @GuardedBy("mTasks")
    private boolean mFinalized;

    private volatile boolean mCanceled;
    private int mIterationIdForTesting = PostTask.sTestIterationForTesting;

    private final Runnable mRunAndPost =
            new Runnable() {
                @Override
                @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
                public void run() {
                    if (mIterationIdForTesting != PostTask.sTestIterationForTesting) {
                        cancel();
                    }
                    if (mCanceled) return;

                    Pair<Integer, Runnable> pair = mTasks.pop();
                    try (TraceEvent e =
                            TraceEvent.scoped(
                                    "ChainedTask.run: " + pair.second.getClass().getName())) {
                        pair.second.run();
                    }
                    if (!mTasks.isEmpty()) PostTask.postTask(mTasks.peek().first, this);
                }
            };

    /**
     * Adds a task to the list of tasks to run. Cannot be called once {@link start()} has been
     * called.
     */
    public void add(@TaskTraits int traits, Runnable task) {
        assert mIterationIdForTesting == PostTask.sTestIterationForTesting;
        if (PostTask.ENABLE_TASK_ORIGINS) {
            task = PostTask.populateTaskOrigin(new TaskOriginException(), task);
        }
        synchronized (mTasks) {
            assert !mFinalized : "Must not call add() after start()";
            mTasks.add(new Pair<>(traits, task));
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
        if (coalesceTasks) {
            @TaskTraits int traits = mTasks.peek().first;
            PostTask.runOrPostTask(
                    traits,
                    () -> {
                        for (Pair<Integer, Runnable> pair : mTasks) {
                            assert PostTask.canRunTaskImmediately(pair.first);
                            pair.second.run();
                            if (mCanceled) return;
                        }
                    });
        } else {
            PostTask.postTask(mTasks.peek().first, mRunAndPost);
        }
    }
}
