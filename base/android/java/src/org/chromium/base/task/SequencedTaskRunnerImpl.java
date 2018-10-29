// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.chromium.base.annotations.JNINamespace;

import javax.annotation.Nullable;

/**
 * Implementation of the abstract class {@link SequencedTaskRunner}. Uses AsyncTasks until
 * native APIs are available.
 */
@JNINamespace("base")
public class SequencedTaskRunnerImpl implements SequencedTaskRunner {
    private final Object mLock = new Object();

    @Nullable
    private final TaskTraits mTaskTraits;
    private long mNativeTaskRunnerAndroid;

    @Nullable
    private PreNativeSequence mPreNativeSequence;

    /**
     * @param traits The TaskTraits associated with this SequencedTaskRunnerImpl.
     */
    SequencedTaskRunnerImpl(TaskTraits traits) {
        mTaskTraits = traits;
    }

    @Override
    protected void finalize() {
        if (mNativeTaskRunnerAndroid != 0) nativeFinalize(mNativeTaskRunnerAndroid);
    }

    @Override
    public void initNativeTaskRunner() {
        synchronized (mLock) {
            mNativeTaskRunnerAndroid = nativeInit(mTaskTraits.mPrioritySetExplicitly,
                    mTaskTraits.mPriority, mTaskTraits.mMayBlock, mTaskTraits.mExtensionId,
                    mTaskTraits.mExtensionData);

            if (mPreNativeSequence != null) mPreNativeSequence.migrateToNative();
        }
    }

    @Override
    public void postTask(Runnable task) {
        synchronized (mLock) {
            if (mNativeTaskRunnerAndroid != 0) {
                nativePostTask(mNativeTaskRunnerAndroid, task);
            } else {
                if (mPreNativeSequence == null) mPreNativeSequence = new PreNativeImpl();
                mPreNativeSequence.postTask(task);
            }
        }
    }

    private class PreNativeImpl extends PreNativeSequence {
        PreNativeImpl() {
            super("SequencedTaskRunnerImpl.PreNativeImpl.run");
        }

        @Override
        protected void scheduleNext() {
            AsyncTask.THREAD_POOL_EXECUTOR.execute(this);
        }

        @Override
        protected void migrateToNative() {
            while (!mTasks.isEmpty()) {
                nativePostTask(mNativeTaskRunnerAndroid, mTasks.poll());
            }
            mPreNativeSequence = null;
        }
    }

    // NB due to Proguard obfuscation it's easiest to pass the traits via arguments.
    private static native long nativeInit(boolean prioritySetExplicitly, int priority,
            boolean mayBlock, byte extensionId, byte[] extensionData);
    private native void nativeFinalize(long nativeTaskRunnerAndroid);
    private native void nativePostTask(long nativeTaskRunnerAndroid, Runnable task);
}
