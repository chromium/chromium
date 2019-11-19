// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Handler;
import android.os.Message;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.JNINamespace;

/**
 * Implementation of the abstract class {@link SingleThreadTaskRunner}. Before native initialization
 * tasks are posted to the {@link java android.os.Handler}, after native initialization they're
 * posted to a base::SingleThreadTaskRunner which runs on the same thread.
 */
@JNINamespace("base")
public class SingleThreadTaskRunnerImpl extends TaskRunnerImpl implements SingleThreadTaskRunner {
    @Nullable
    private final Handler mHandler;
    private final boolean mPostTaskAtFrontOfQueue;

    /**
     * @param handler                The backing Handler if any. Note this must run tasks on the
     *                               same thread that the native code runs a task with |traits|.
     *                               If handler is null then tasks won't run until native has
     *                               initialized.
     * @param traits                 The TaskTraits associated with this SingleThreadTaskRunnerImpl.
     * @param postTaskAtFrontOfQueue If true, tasks posted to the backing Handler will be posted at
     *                               the front of the queue.
     */
    public SingleThreadTaskRunnerImpl(
            Handler handler, TaskTraits traits, boolean postTaskAtFrontOfQueue) {
        super(traits, "SingleThreadTaskRunnerImpl", TaskRunnerType.SINGLE_THREAD);
        mHandler = handler;
        mPostTaskAtFrontOfQueue = postTaskAtFrontOfQueue;
    }

    public SingleThreadTaskRunnerImpl(Handler handler, TaskTraits traits) {
        this(handler, traits, false);
    }

    @Override
    public boolean belongsToCurrentThread() {
        synchronized (mLock) {
            if (mNativeTaskRunnerAndroid != 0)
                return TaskRunnerImplJni.get().belongsToCurrentThread(mNativeTaskRunnerAndroid);
        }
        if (mHandler != null) return mHandler.getLooper().getThread() == Thread.currentThread();
        assert (false);
        return false;
    }

    @Override
    protected void schedulePreNativeTask() {
        // if |mHandler| is null then pre-native task execution is not supported.
        if (mHandler == null) {
            return;
        } else if (mPostTaskAtFrontOfQueue) {
            postAtFrontOfQueue();
        } else {
            mHandler.post(mRunPreNativeTaskClosure);
        }
    }

    @SuppressLint("NewApi")
    private void postAtFrontOfQueue() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // The mHandler.postAtFrontOfQueue() API uses fences which batches messages up per
            // frame. We want to bypass that for performance, hence we use async messages where
            // possible.
            Message message = Message.obtain(mHandler, mRunPreNativeTaskClosure);
            message.setAsynchronous(true);
            mHandler.sendMessageAtFrontOfQueue(message);
        } else {
            mHandler.postAtFrontOfQueue(mRunPreNativeTaskClosure);
        }
    }
}
