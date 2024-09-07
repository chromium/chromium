// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test class for {@link PostTask}.
 *
 * <p>Note due to layering concerns we can't test post native functionality in a base javatest.
 * Instead see: content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * NativePostTaskTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PostTaskTest {
    @Test
    @SmallTest
    public void testPreNativePostTask() throws TimeoutException {
        // This test should not timeout.
        CallbackHelper callbackHelper = new CallbackHelper();
        PostTask.postTask(TaskTraits.USER_BLOCKING, callbackHelper::notifyCalled);
        callbackHelper.waitForOnly();
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
    public void testSyncException() {
        RuntimeException ex =
                Assert.assertThrows(
                        RuntimeException.class,
                        () -> {
                            PostTask.runSynchronously(
                                    TaskTraits.USER_BLOCKING,
                                    () -> {
                                        throw new Error("Error");
                                    });
                        });
        Assert.assertEquals("Error", ex.getCause().getMessage());
        // Ensure no TaskOriginException.
        Assert.assertNull("Was " + Log.getStackTraceString(ex), ex.getCause().getCause());
    }

    @Test
    @SmallTest
    public void testAsyncException() throws TimeoutException {
        AtomicReference<Throwable> uncaught = new AtomicReference<>();
        CallbackHelper callbackHelper = new CallbackHelper();
        PostTask.postTask(
                TaskTraits.USER_BLOCKING,
                () -> {
                    Thread.currentThread()
                            .setUncaughtExceptionHandler(
                                    (thread, throwable) -> {
                                        uncaught.set(throwable);
                                        callbackHelper.notifyCalled();
                                    });
                    throw new Error("Error");
                });

        callbackHelper.waitForOnly();
        Throwable ex = uncaught.get();
        Assert.assertEquals("Error", ex.getMessage());
        Throwable actualTaskOrigin = ex.getCause();
        String assertMsg = "Was: " + Log.getStackTraceString(ex);
        if (PostTask.ENABLE_TASK_ORIGINS) {
            Assert.assertTrue(assertMsg, actualTaskOrigin instanceof TaskOriginException);
        } else {
            Assert.assertNull(assertMsg, actualTaskOrigin);
        }
    }
}
