// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.jni_zero.JNINamespace;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/** TaskRunner for UI thread. */
@NullMarked
@JNINamespace("base")
public class UiThreadTaskRunnerImpl extends TaskRunnerImpl implements SequencedTaskRunner {

    /**
     * @param traits The TaskTraits associated with this TaskRunner.
     */
    public UiThreadTaskRunnerImpl(@TaskTraits int traits) {
        super(traits, "UiThreadTaskRunner", TaskRunnerType.SINGLE_THREAD);
    }

    /**
     * Tasks are only removed from the pre-native task queue once run. So we can safely ignore them
     * if preNativeUiTasks are disabled. They will be sent off to native task runners when native is
     * initialized, where they will be scheduled as normal.
     */
    @Override
    protected void schedulePreNativeTask() {
        if (PostTask.canRunUiTaskBeforeNativeInit(mTaskTraits)) {
            ThreadUtils.getUiThreadHandler().post(mRunPreNativeTaskClosure);
        }
    }

    /**
     * Tasks are only removed from the pre-native task queue once run. So we can safely ignore them
     * if preNativeUiTasks are disabled. They will be sent off to native task runners when native is
     * initialized, where they will be scheduled as normal.
     */
    @Override
    protected boolean schedulePreNativeDelayedTask(Runnable task, long delay) {
        if (PostTask.canRunUiTaskBeforeNativeInit(mTaskTraits)) {
            ThreadUtils.getUiThreadHandler().postDelayed(task, delay);
            return true;
        }
        return false;
    }
}
