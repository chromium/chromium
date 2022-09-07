// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Implementation of the abstract class {@link SequencedTaskRunner}. Uses AsyncTasks until
 * native APIs are available.
 */
public class SequencedTaskRunnerImpl extends TaskRunnerImpl implements SequencedTaskRunner {
    private AtomicInteger mPendingTasks = new AtomicInteger();

    private volatile boolean mReadyToCreateNativeTaskRunner;

    /**
     * @param traits The TaskTraits associated with this SequencedTaskRunnerImpl.
     */
    SequencedTaskRunnerImpl(TaskTraits traits) {
        super(traits, "SequencedTaskRunnerImpl", TaskRunnerType.SEQUENCED);
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
            if (!mReadyToCreateNativeTaskRunner) {
                // Kick off execution in the pre-native pool.
                super.schedulePreNativeTask();
            } else {
                // Initialize native runner so it can take over tasks in queue.
                super.initNativeTaskRunner();
            }
        }
    }

    @Override
    void initNativeTaskRunner() {
        mReadyToCreateNativeTaskRunner = true;
        // There are two possibilities:
        // 1. There is no task currently running - native runner is initialized immediately.
        //    Incrementing mPendingTaskCounter prevents concurrent calls to post(Delayed)Task
        //    from starting task in pre-native pool while native runner is being initialized.
        // 2. There is a task currently running in pre-native pool. The native runner will be
        //    initialized when the task is completed.
        if (mPendingTasks.getAndIncrement() == 0) {
            super.initNativeTaskRunner();
        }
    }
}
