// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Implementation of the abstract class {@link TaskRunnerImpl}. Uses AsyncTasks until
 * native APIs are available.
 */
@JNINamespace("base")
public class TaskRunnerImpl implements TaskRunner {
    @Nullable
    private final TaskTraits mTaskTraits;
    private final Object mLock = new Object();
    private long mNativeTaskRunnerAndroid;

    @Nullable
    private List<PreNativeTask> mPreNativeTasks = new ArrayList<>();

    /**
     * @param traits The TaskTraits associated with this TaskRunnerImpl.
     */
    TaskRunnerImpl(TaskTraits traits) {
        mTaskTraits = traits;
    }

    @Override
    protected void finalize() {
        if (mNativeTaskRunnerAndroid != 0) nativeFinalize(mNativeTaskRunnerAndroid);
    }

    @Override
    public void postTask(Runnable task) {
        synchronized (mLock) {
            if (mNativeTaskRunnerAndroid != 0) {
                nativePostTask(mNativeTaskRunnerAndroid, task);
                return;
            }

            // We don't expect a whole lot of these, if that changes consider pooling them.
            PreNativeTask preNativeTask = new PreNativeTask(task);
            mPreNativeTasks.add(preNativeTask);
            AsyncTask.THREAD_POOL_EXECUTOR.execute(preNativeTask);
        }
    }

    private class PreNativeTask implements Runnable {
        PreNativeTask(Runnable task) {
            this.mTask = task;
        }

        @Override
        public void run() {
            try (TraceEvent te = TraceEvent.scoped("TaskRunnerImpl.PreNativeTask.run")) {
                synchronized (mLock) {
                    if (mPreNativeTasks == null) return;

                    mPreNativeTasks.remove(this);
                }

                mTask.run();
            }
        }

        final Runnable mTask;
    }

    @Override
    public void initNativeTaskRunner() {
        synchronized (mLock) {
            mNativeTaskRunnerAndroid = nativeInit(mTaskTraits.mPrioritySetExplicitly,
                    mTaskTraits.mPriority, mTaskTraits.mMayBlock, mTaskTraits.mExtensionId,
                    mTaskTraits.mExtensionData);
            for (PreNativeTask task : mPreNativeTasks) {
                nativePostTask(mNativeTaskRunnerAndroid, task.mTask);
            }
            mPreNativeTasks = null;
        }
    }

    // NB due to Proguard obfuscation it's easiest to pass the traits via arguments.
    private static native long nativeInit(boolean prioritySetExplicitly, int priority,
            boolean mayBlock, byte extensionId, byte[] extensionData);
    private native void nativeFinalize(long nativeTaskRunnerAndroid);
    private native void nativePostTask(long nativeTaskRunnerAndroid, Runnable task);
}
