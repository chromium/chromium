// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import java.util.concurrent.atomic.AtomicInteger;

import javax.annotation.concurrent.GuardedBy;

/**
 * Implementation of the abstract class {@link SequencedTaskRunner}. Uses AsyncTasks until
 * native APIs are available.
 */
public class SequencedTaskRunnerImpl extends TaskRunnerImpl implements SequencedTaskRunner {
    private AtomicInteger mPendingTasks = new AtomicInteger();
    @GuardedBy("mLock")
    private int mNumUnfinishedNativeTasks;

    /**
     * @param traits The TaskTraits associated with this SequencedTaskRunnerImpl.
     */
    SequencedTaskRunnerImpl(TaskTraits traits) {
        super(traits, "SequencedTaskRunnerImpl", TaskRunnerType.SEQUENCED);
        disableLifetimeCheck();
    }

    @Override
    public void initNativeTaskRunner() {
        synchronized (mLock) {
            migratePreNativeTasksToNative();
        }
    }

    @Override
    protected void schedulePreNativeTask() {
        if (mPendingTasks.getAndIncrement() == 0) {
            super.schedulePreNativeTask();
        }
    }

    @Override
    protected void runPreNativeTask() {
        super.runPreNativeTask();
        if (mPendingTasks.decrementAndGet() > 0) {
            super.schedulePreNativeTask();
        }
    }

    @Override
    public void postDelayedTaskToNative(Runnable runnable, long delay) {
        synchronized (mLock) {
            if (mNumUnfinishedNativeTasks++ == 0) {
                initNativeTaskRunnerInternal();
            }
            Runnable r = () -> {
                // No need for try/finally since exceptions here will kill the app entirely.
                runnable.run();
                synchronized (mLock) {
                    if (--mNumUnfinishedNativeTasks == 0) {
                        destroyInternal();
                    }
                }
            };
            super.postDelayedTaskToNative(r, delay);
        }
    }
}
