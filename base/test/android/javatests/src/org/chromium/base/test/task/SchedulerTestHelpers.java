// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.task;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import org.chromium.base.task.TaskRunner;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Collection of helpers for testing the java PostTask.
 */
public class SchedulerTestHelpers {
    public static void postRecordOrderTask(
            TaskRunner taskQueue, List<Integer> orderList, int order) {
        postRecordOrderDelayedTask(taskQueue, orderList, order, 0);
    }

    public static void postRecordOrderDelayedTask(
            TaskRunner taskQueue, List<Integer> orderList, int order, long delay) {
        taskQueue.postDelayedTask(new Runnable() {
            @Override
            public void run() {
                orderList.add(order);
            }
        }, delay);
    }

    public static void postTaskAndBlockUntilRun(TaskRunner taskQueue) {
        postDelayedTaskAndBlockUntilRun(taskQueue, 0);
    }

    public static void postDelayedTaskAndBlockUntilRun(TaskRunner taskQueue, long delay) {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        taskQueue.postDelayedTask(new Runnable() {
            @Override
            public void run() {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            }
        }, delay);
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

    public static void postThreeTasksInOrder(TaskRunner taskQueue, List<Integer> orderList) {
        postRecordOrderTask(taskQueue, orderList, 1);
        postRecordOrderTask(taskQueue, orderList, 2);
        postRecordOrderTask(taskQueue, orderList, 3);
    }

    public static void postThreeDelayedTasksInOrder(TaskRunner taskQueue, List<Integer> orderList) {
        postRecordOrderDelayedTask(taskQueue, orderList, 1, 1);
        postRecordOrderDelayedTask(taskQueue, orderList, 2, 1);
        postRecordOrderDelayedTask(taskQueue, orderList, 3, 1);
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
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();

        new Handler(handlerThread.getLooper()).post(() -> {
            Looper.myQueue().addIdleHandler(() -> {
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
                return false;
            });
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
