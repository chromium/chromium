// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;
import static org.junit.Assert.assertNotNull;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test class for {@link PostTask}.
 *
 * Note due to layering concerns we can't test post native functionality in a
 * base javatest. Instead see:
 * content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * TaskSchedulerTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
@MinAndroidSdkLevel(23)
@TargetApi(Build.VERSION_CODES.M)
public class PostTaskTest {
    @Test
    @SmallTest
    public void testPreNativePostTask() {
        // This test should not timeout.
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        PostTask.postTask(new TaskTraits(), new Runnable() {
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
    public void testCreateSingleThreadTaskRunner() throws Exception {
        TaskRunner taskQueue = PostTask.createSingleThreadTaskRunner(new TaskTraits());
        // A SingleThreadTaskRunner with default traits will run in the native thread pool
        // and tasks posted won't run until after the native library has loaded.
        assertNotNull(taskQueue);
    }

    @Test
    @SmallTest
    public void testCreateSequencedTaskRunner() throws Exception {
        TaskRunner taskQueue = PostTask.createSequencedTaskRunner(new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);

        assertThat(orderList, contains(1, 2, 3));
    }

    @Test
    @SmallTest
    public void testCreateTaskRunner() throws Exception {
        TaskRunner taskQueue = PostTask.createTaskRunner(new TaskTraits());

        // This should not timeout.
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);
    }
}
