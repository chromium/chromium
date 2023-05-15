// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Handler;

/**
 * This {@link TaskExecutor} is for tasks posted with UI Thread {@link TaskTraits}. It maps to
 * content::BrowserTaskExecutor in C++, except that in Java the UI thread is a base/ concept and
 * only UI thread posting is supported.
 */
public class UiThreadTaskExecutor implements TaskExecutor {
    private static boolean sRegistered;

    private final SingleThreadTaskRunner mBestEffortTaskRunner;
    private final SingleThreadTaskRunner mUserVisibleTaskRunner;
    private final SingleThreadTaskRunner mUserBlockingTaskRunner;

    public UiThreadTaskExecutor(Handler handler) {
        mBestEffortTaskRunner = new SingleThreadTaskRunnerImpl(handler, TaskTraits.UI_BEST_EFFORT);
        mUserVisibleTaskRunner =
                new SingleThreadTaskRunnerImpl(handler, TaskTraits.UI_USER_VISIBLE);
        mUserBlockingTaskRunner =
                new SingleThreadTaskRunnerImpl(handler, TaskTraits.UI_USER_BLOCKING);
    }

    @Override
    public TaskRunner createTaskRunner(@TaskTraits int taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(@TaskTraits int taskTraits) {
        return createSingleThreadTaskRunner(taskTraits);
    }

    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(@TaskTraits int taskTraits) {
        if (TaskTraits.UI_BEST_EFFORT == taskTraits) {
            return mBestEffortTaskRunner;
        } else if (TaskTraits.UI_USER_VISIBLE == taskTraits) {
            return mUserVisibleTaskRunner;
        } else if (TaskTraits.UI_USER_BLOCKING == taskTraits) {
            return mUserBlockingTaskRunner;
        } else {
            // Add support for additional TaskTraits here if encountering this exception.
            throw new RuntimeException();
        }
    }

    @Override
    public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
        createSingleThreadTaskRunner(taskTraits).postDelayedTask(task, delay);
    }

    @Override
    public boolean canRunTaskImmediately(@TaskTraits int traits) {
        return createSingleThreadTaskRunner(traits).belongsToCurrentThread();
    }
}
