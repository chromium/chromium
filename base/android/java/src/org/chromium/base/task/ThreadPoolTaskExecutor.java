// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

/**
 * The {@link TaskExecutor} for ThreadPool tasks.
 * TODO(crbug.com/1026641): Provide direct Java APIs for ThreadPool vs UI thread
 * task posting instead of sharding based on {@link TaskTraits}.
 */
class ThreadPoolTaskExecutor implements TaskExecutor {
    private static final int TRAITS_COUNT =
            TaskTraits.THREAD_POOL_TRAITS_END - TaskTraits.THREAD_POOL_TRAITS_START + 1;
    private final TaskRunner mTraitsToRunnerMap[] = new TaskRunner[TRAITS_COUNT];

    public ThreadPoolTaskExecutor() {
        for (int i = 0; i < TRAITS_COUNT; i++) {
            mTraitsToRunnerMap[i] = createTaskRunner(TaskTraits.THREAD_POOL_TRAITS_START + i);
        }
    }

    @Override
    public TaskRunner createTaskRunner(@TaskTraits int taskTraits) {
        return new TaskRunnerImpl(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(@TaskTraits int taskTraits) {
        return new SequencedTaskRunnerImpl(taskTraits);
    }

    /**
     * If CurrentThread is not specified, or we are being called from within a threadpool task
     * this maps to a single thread within the native thread pool. If so we can't run tasks
     * posted on it until native has started.
     */
    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(@TaskTraits int taskTraits) {
        // Tasks posted via this API will not execute until after native has started.
        return new SingleThreadTaskRunnerImpl(null, taskTraits);
    }

    @Override
    public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
        int index = taskTraits - TaskTraits.THREAD_POOL_TRAITS_START;
        mTraitsToRunnerMap[index].postDelayedTask(task, delay);
    }

    @Override
    public boolean canRunTaskImmediately(@TaskTraits int traits) {
        return false;
    }
}
