// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.task;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.MessageQueue;

import org.chromium.base.task.TaskRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Collection of helpers for testing the java PostTask.
 */
@MinAndroidSdkLevel(23)
@TargetApi(Build.VERSION_CODES.M)
public class SchedulerTestHelpers {
    public static void postRecordOrderTask(
            TaskRunner taskQueue, List<Integer> orderList, int order) {
        taskQueue.postTask(new Runnable() {
            @Override
            public void run() {
                orderList.add(order);
            }
        });
    }

    public static void postTaskAndBlockUntilRun(TaskRunner taskQueue) {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        taskQueue.postTask(new Runnable() {
            @Override
            public void run() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            }
        });
        synchronized (lock) {
            try {
                while (!taskExecuted.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }

    /**
     * A helper which posts a task on the handler which when run blocks until unblock() is called.
     */
    public static class HandlerBlocker {
        final Handler mHandler;
        final Object mLock = new Object();
        final AtomicBoolean mTaskExecuted = new AtomicBoolean();

        public HandlerBlocker(Handler handler) {
            mHandler = handler;
        }

        /**
         * Posts a task that blocks until unblock() is called.
         */
        public void postBlockingTask() {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    synchronized (mLock) {
                        try {
                            while (!mTaskExecuted.get()) {
                                mLock.wait();
                            }
                        } catch (InterruptedException ie) {
                            ie.printStackTrace();
                        }
                    }
                }
            });
        }

        public void unblock() {
            synchronized (mLock) {
                mTaskExecuted.set(true);
                mLock.notify();
            }
        }
    };

    /**
     * Waits until the looper's MessageQueue becomes idle.
     */
    public static void preNativeRunUntilIdle(HandlerThread handlerThread) {
        final MessageQueue messageQueue = handlerThread.getLooper().getQueue();
        // This API was added in sdk level 23.
        if (messageQueue.isIdle()) {
            return;
        }
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        messageQueue.addIdleHandler(new MessageQueue.IdleHandler() {
            @Override
            public boolean queueIdle() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
                return false;
            }
        });
        synchronized (lock) {
            try {
                while (!taskExecuted.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }
}
