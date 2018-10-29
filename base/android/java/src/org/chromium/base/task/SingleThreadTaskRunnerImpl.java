// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Handler;

import org.chromium.base.annotations.JNINamespace;

import javax.annotation.Nullable;

/**
 * Implementation of the abstract class {@link SingleThreadTaskRunner}. Before native initialization
 * tasks are posted to the {@link java android.os.Handler}, after native initialization they're
 * posted to a base::SingleThreadTaskRunner which runs on the same thread.
 */
@JNINamespace("base")
public class SingleThreadTaskRunnerImpl implements SingleThreadTaskRunner {
    private final Object mLock = new Object();

    @Nullable
    private final Handler mHandler;

    @Nullable
    private final TaskTraits mTaskTraits;
    private long mNativeTaskRunnerAndroid;

    @Nullable
    private PreNativeSequence mPreNativeSequence;

    /**
     * @param handler The backing Handler if any. Note this must run tasks on the same thread that
     * the native code runs a task with |traits|.  If handler is null then tasks won't run until
     * native has initialized.
     * @param traits The TaskTraits associated with this SingleThreadTaskRunnerImpl.
     */
    public SingleThreadTaskRunnerImpl(Handler handler, TaskTraits traits) {
        mHandler = handler;
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
            super("SingleThreadTaskRunnerImpl.PreNativeImpl.run");
        }

        @Override
        protected void scheduleNext() {
            // if |mHandler| is null then pre-native task execution is not supported.
            if (mHandler != null) mHandler.post(this);
        }

        /**
         * This is only called in the context of PreNativeSequence.initNativeTaskRunner and
         * PreNativeSequence.postTask which are synchronized hence this doesn't need additional
         * synchronization.
         */
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
