// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.chromium.base.TraceEvent;

import java.util.ArrayDeque;

/**
 * Common implementation of a pre-native task runner which runs on either a thread pool or
 * on a Handler, which can migrate posted tasks to native APIs.
 */
abstract class PreNativeSequence implements Runnable {
    protected final ArrayDeque<Runnable> mTasks = new ArrayDeque<Runnable>();
    private final String mTraceCategory;
    private boolean mTaskRunning;
    private boolean mMigrateToNative;

    /**
     * @param traceCategory The trace category to use when emitting trace events for tasks posted
     * on this sequence.
     */
    PreNativeSequence(String traceCategory) {
        mTraceCategory = traceCategory;
    }

    /**
     * @param task The task to be enqueued and run sequentially.
     */
    synchronized void postTask(Runnable task) {
        boolean wasEmpty = mTasks.isEmpty();
        mTasks.offer(task);

        if (wasEmpty && !mTaskRunning) scheduleNext();
    }

    /**
     * Ensures the next task from this sequence is run via native APIs.
     */
    synchronized void requestMigrateToNative() {
        if (mTaskRunning) {
            mMigrateToNative = true;
        } else {
            migrateToNative();
        }
    }

    @Override
    public void run() {
        try (TraceEvent te = TraceEvent.scoped(mTraceCategory)) {
            Runnable activeTask;
            synchronized (this) {
                if (mMigrateToNative) {
                    migrateToNative();
                    return;
                }

                activeTask = mTasks.poll();
                mTaskRunning = true;
            }

            // Avoid holding the lock for too long, to allow other threads to postTask.
            if (activeTask != null) activeTask.run();

            synchronized (this) {
                mTaskRunning = false;
                if (!mTasks.isEmpty()) scheduleNext();
            }
        }
    }

    protected abstract void scheduleNext();

    protected abstract void migrateToNative();
}
