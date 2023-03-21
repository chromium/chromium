// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

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
        return new TaskRunnerImpl(taskTraits);
    }

    @Override
    public SequencedTaskRunner createSequencedTaskRunner(TaskTraits taskTraits) {
        return new SequencedTaskRunnerImpl(taskTraits);
    }

    /**
     * If CurrentThread is not specified, or we are being called from within a threadpool task
     * this maps to a single thread within the native thread pool. If so we can't run tasks
     * posted on it until native has started.
     */
    @Override
    public SingleThreadTaskRunner createSingleThreadTaskRunner(TaskTraits taskTraits) {
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

    // Git doesn't want to detect this file as moved, so I'm adding some arbitrary
    // comment lines to get the similarity detection up.
    //
    // Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut
    // labore et dolore magna aliqua. Purus in mollis nunc sed id semper risus in hendrerit. Aliquam
    // id diam maecenas ultricies mi eget mauris. In eu mi bibendum neque egestas. Habitant morbi
    // tristique senectus et netus et malesuada fames ac. Non blandit massa enim nec dui nunc
    // mattis. Ut tristique et egestas quis ipsum. Mauris rhoncus aenean vel elit scelerisque.
    // Ridiculus mus mauris vitae ultricies leo integer malesuada nunc. Sed arcu non odio euismod.
    // Vulputate mi sit amet mauris commodo quis. Lobortis scelerisque fermentum dui faucibus. Urna
    // cursus eget nunc scelerisque viverra. Tempus quam pellentesque nec nam aliquam sem et tortor
    // consequat. Iaculis eu non diam phasellus vestibulum. Lectus quam id leo in vitae turpis.
    // Lacinia quis vel eros donec ac odio tempor.
}
