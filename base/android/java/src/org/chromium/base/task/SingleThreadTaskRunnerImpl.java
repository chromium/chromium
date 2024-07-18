// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.jni_zero.JNINamespace;

/**
 * Implementation of the abstract class {@link SingleThreadTaskRunner}. Before native initialization
 * tasks are posted to the {@link java android.os.Handler}, after native initialization they're
 * posted to a base::SingleThreadTaskRunner which runs on the same thread.
 */
@JNINamespace("base")
public class SingleThreadTaskRunnerImpl extends TaskRunnerImpl implements SingleThreadTaskRunner {
    /**
     * @param traits The TaskTraits associated with this TaskRunner.
     */
    public SingleThreadTaskRunnerImpl(@TaskTraits int traits) {
        super(traits, "SingleThreadTaskRunner", TaskRunnerType.SINGLE_THREAD);
    }

    @Override
    public boolean belongsToCurrentThread() {
        return belongsToCurrentThreadInternal();
    }

    @Override
    protected void schedulePreNativeTask() {
        // Pre-native task execution is not supported.
    }

    @Override
    protected boolean schedulePreNativeDelayedTask(Runnable task, long delay) {
        // Pre-native task execution is not supported.
        return false;
    }
}
