// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Handler;
import android.os.Message;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Implementation of the abstract class {@link SingleThreadTaskRunner}. Before native initialization
 * tasks are posted to the {@link java android.os.Handler}, after native initialization they're
 * posted to a base::SingleThreadTaskRunner which runs on the same thread.
 */
@JNINamespace("base")
public class SingleThreadTaskRunnerImpl extends TaskRunnerImpl implements SingleThreadTaskRunner {
    @Nullable
    private final Handler mHandler;
    private final boolean mPostPreNativeTasksAtFrontOfQueue;

    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({PreNativeTaskPostType.POSTED_AT_BACK_OF_QUEUE,
            PreNativeTaskPostType.POSTED_AT_FRONT_OF_QUEUE,
            PreNativeTaskPostType.DEFERRED_TO_NATIVE_INIT, PreNativeTaskPostType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PreNativeTaskPostType {
        int POSTED_AT_BACK_OF_QUEUE = 0;
        int POSTED_AT_FRONT_OF_QUEUE = 1;
        int DEFERRED_TO_NATIVE_INIT = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * @param handler                The backing Handler if any. Note this must run tasks on the
     *                               same thread that the native code runs a task with |traits|.
     *                               If handler is null then tasks won't run until native has
     *                               initialized.
     * @param traits                 The TaskTraits associated with this SingleThreadTaskRunnerImpl.
     * @param postPreNativeTasksAtFrontOfQueue If true, tasks posted to the backing Handler (i.e.,
     *                               before native initialization) will be posted at the front of
     *                               the queue.
     */
    public SingleThreadTaskRunnerImpl(
            Handler handler, TaskTraits traits, boolean postPreNativeTasksAtFrontOfQueue) {
        super(traits, "SingleThreadTaskRunnerImpl", TaskRunnerType.SINGLE_THREAD);
        mHandler = handler;
        mPostPreNativeTasksAtFrontOfQueue = postPreNativeTasksAtFrontOfQueue;
    }

    public SingleThreadTaskRunnerImpl(Handler handler, TaskTraits traits) {
        this(handler, traits, false);
    }

    @Override
    public boolean belongsToCurrentThread() {
        Boolean belongs = belongsToCurrentThreadInternal();
        if (belongs != null) return belongs.booleanValue();
        assert mHandler != null;
        return mHandler.getLooper().getThread() == Thread.currentThread();
    }

    @Override
    protected void schedulePreNativeTask() {
        // if |mHandler| is null then pre-native task execution is not supported.
        if (mHandler == null) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TaskScheduling.PreNativeTaskPostType",
                    PreNativeTaskPostType.DEFERRED_TO_NATIVE_INIT,
                    PreNativeTaskPostType.NUM_ENTRIES);
            return;
        } else if (mPostPreNativeTasksAtFrontOfQueue) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TaskScheduling.PreNativeTaskPostType",
                    PreNativeTaskPostType.POSTED_AT_FRONT_OF_QUEUE,
                    PreNativeTaskPostType.NUM_ENTRIES);
            postAtFrontOfQueue();
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TaskScheduling.PreNativeTaskPostType",
                    PreNativeTaskPostType.POSTED_AT_BACK_OF_QUEUE,
                    PreNativeTaskPostType.NUM_ENTRIES);
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
