// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.view.Choreographer;

import org.chromium.base.ThreadUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * The default {@link TaskExecutor}.
 * TODO(crbug.com/1026641): Provide direct Java APIs for ThreadPool vs UI thread
 * task posting instead of sharding based on {@link TaskTraits}.
 */
class DefaultTaskExecutor implements TaskExecutor {
    private final Map<TaskTraits, TaskRunner> mTraitsToRunnerMap = new HashMap<>();

    @Override
    public TaskRunner createTaskRunner(TaskTraits taskTraits) {
        if (taskTraits.mIsChoreographerFrame) return createChoreographerTaskRunner();
        return new TaskRunnerImpl(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        if (taskTraits.mIsChoreographerFrame) return createChoreographerTaskRunner();
        return new SequencedTaskRunnerImpl(taskTraits);
    }

    /**
     * If CurrentThread is not specified, or we are being called from within a threadpool task
     * this maps to a single thread within the native thread pool. If so we can't run tasks
     * posted on it until native has started.
     */
    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
        if (taskTraits.mIsChoreographerFrame) return createChoreographerTaskRunner();
        // Tasks posted via this API will not execute until after native has started.
        return new SingleThreadTaskRunnerImpl(null, taskTraits);
    }

    @Override
    public synchronized void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
        if (taskTraits.hasExtension()) {
            createTaskRunner(taskTraits).postDelayedTask(task, delay);
        } else {
            // Caching TaskRunners only for common TaskTraits.
            TaskRunner runner = mTraitsToRunnerMap.get(taskTraits);
            if (runner == null) {
                runner = createTaskRunner(taskTraits);
                mTraitsToRunnerMap.put(taskTraits, runner);
            }
            runner.postDelayedTask(task, delay);
        }
    }

    @Override
    public boolean canRunTaskImmediately(TaskTraits traits) {
        return false;
    }

    private synchronized ChoreographerTaskRunner createChoreographerTaskRunner() {
        // TODO(alexclarke): Migrate to the new Android UI thread trait when available.
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return new ChoreographerTaskRunner(Choreographer.getInstance()); });
    }
}
