// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.jni_zero.JNINamespace;

import org.chromium.base.ThreadUtils;

/** TaskRunner for UI thread. */
@JNINamespace("base")
public class UiThreadTaskRunnerImpl extends TaskRunnerImpl implements SequencedTaskRunner {
    /**
     * @param traits The TaskTraits associated with this TaskRunner.
     */
    public UiThreadTaskRunnerImpl(@TaskTraits int traits) {
        super(traits, "UiThreadTaskRunner", TaskRunnerType.SINGLE_THREAD);
    }

    @Override
    protected void schedulePreNativeTask() {
        ThreadUtils.getUiThreadHandler().post(mRunPreNativeTaskClosure);
    }

    @Override
    protected boolean schedulePreNativeDelayedTask(Runnable task, long delay) {
        ThreadUtils.getUiThreadHandler().postDelayed(task, delay);
        return true;
    }
}
