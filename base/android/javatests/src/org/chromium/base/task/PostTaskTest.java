// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;
import static org.junit.Assert.assertNotNull;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test class for {@link PostTask}.
 *
 * Note due to layering concerns we can't test post native functionality in a
 * base javatest. Instead see:
 * content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * NativePostTaskTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PostTaskTest {
    @Test
    @SmallTest
    public void testPreNativePostTask() {
        // This test should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(TaskTraits.USER_BLOCKING, new Runnable() {
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

    @Test
    @SmallTest
    public void testCreateSingleThreadTaskRunner() {
        TaskRunner taskQueue = PostTask.createSingleThreadTaskRunner(TaskTraits.USER_BLOCKING);
        // A SingleThreadTaskRunner with default traits will run in the native thread pool
        // and tasks posted won't run until after the native library has loaded.
        assertNotNull(taskQueue);
    }

    @Test
    @SmallTest
    public void testCreateSequencedTaskRunner() {
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);

        assertThat(orderList, contains(1, 2, 3));
    }

    @Test
    @SmallTest
    public void testCreateTaskRunner() {
        TaskRunner taskQueue = PostTask.createTaskRunner(TaskTraits.USER_BLOCKING);

        // This should not timeout.
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);
    }

    @Test
    @SmallTest
    public void testChoreographerFrameTrait() throws Exception {
        List<Integer> orderList = new ArrayList<>();
        CountDownLatch latch = new CountDownLatch(2);
        PostTask.postTask(TaskTraits.CHOREOGRAPHER_FRAME, new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                synchronized (orderList) {
                    orderList.add(1);
                    latch.countDown();
                }
            }
        });

        PostTask.postTask(TaskTraits.CHOREOGRAPHER_FRAME, new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                synchronized (orderList) {
                    orderList.add(2);
                    latch.countDown();
                }
            }
        });

        latch.await();

        assertThat(orderList, contains(1, 2));
    }
}
