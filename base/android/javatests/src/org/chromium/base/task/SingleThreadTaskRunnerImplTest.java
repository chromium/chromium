// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;

import java.util.ArrayList;
import java.util.List;

/**
 * Test class for {@link SingleThreadTaskRunnerImpl}.
 *
 * Note due to layering concerns we can't test post native functionality in a
 * base javatest. Instead see:
 * content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * NativePostTaskTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SingleThreadTaskRunnerImplTest {
    @Before
    public void setUp() {
        mHandlerThread = new HandlerThread("SingleThreadTaskRunnerImplTest thread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    @After
    public void tearDown() throws InterruptedException {
        Looper looper = mHandlerThread.getLooper();
        if (looper != null) {
            looper.quitSafely();
        }
        mHandlerThread.join();
    }

    private HandlerThread mHandlerThread;
    private Handler mHandler;

    @Test
    @SmallTest
    public void testPreNativePostTask() {
        TaskRunner taskQueue =
                new SingleThreadTaskRunnerImpl(mHandler, TaskTraits.USER_BLOCKING, false);
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);

        SchedulerTestHelpers.preNativeRunUntilIdle(mHandlerThread);
        assertThat(orderList, contains(1, 2, 3));
    }

    @Test
    @SmallTest
    public void testBelongsToCurrentThread() {
        // The handler created during test setup belongs to a different thread.
        SingleThreadTaskRunner taskQueue =
                new SingleThreadTaskRunnerImpl(mHandler, TaskTraits.USER_BLOCKING, false);
        Assert.assertFalse(taskQueue.belongsToCurrentThread());

        // We create a handler belonging to current thread.
        Looper.prepare();
        SingleThreadTaskRunner taskQueueCurrentThread =
                new SingleThreadTaskRunnerImpl(new Handler(), TaskTraits.USER_BLOCKING, false);
        Assert.assertTrue(taskQueueCurrentThread.belongsToCurrentThread());
    }

    @Test
    @SmallTest
    public void testPrioritization() {
        TaskRunner highPriorityQueue =
                new SingleThreadTaskRunnerImpl(mHandler, TaskTraits.USER_BLOCKING, true);
        TaskRunner lowPriorityQueue =
                new SingleThreadTaskRunnerImpl(mHandler, TaskTraits.USER_BLOCKING, false);
        List<Integer> orderList = new ArrayList<>();
        // We want to post all these tasks atomically but we're not on the mHandlerThread so
        // we need to post a task to do that for us.
        lowPriorityQueue.postTask(new Runnable() {
            @Override
            public void run() {
                SchedulerTestHelpers.postRecordOrderTask(lowPriorityQueue, orderList, 1);
                SchedulerTestHelpers.postRecordOrderTask(lowPriorityQueue, orderList, 2);
                SchedulerTestHelpers.postRecordOrderTask(lowPriorityQueue, orderList, 3);
                SchedulerTestHelpers.postRecordOrderTask(highPriorityQueue, orderList, 10);
                SchedulerTestHelpers.postRecordOrderTask(highPriorityQueue, orderList, 20);
                SchedulerTestHelpers.postRecordOrderTask(highPriorityQueue, orderList, 30);
            }
        });

        SchedulerTestHelpers.preNativeRunUntilIdle(mHandlerThread);
        assertThat(orderList, contains(10, 20, 30, 1, 2, 3));
    }
}
