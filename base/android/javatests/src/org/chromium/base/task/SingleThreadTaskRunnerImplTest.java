// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.ArrayList;
import java.util.List;

/**
 * Test class for {@link SingleThreadTaskRunnerImpl}.
 *
 * Note due to layering concerns we can't test post native functionality in a
 * base javatest. Instead see:
 * content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * TaskSchedulerTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
@MinAndroidSdkLevel(23)
@TargetApi(Build.VERSION_CODES.M)
public class SingleThreadTaskRunnerImplTest {
    @Before
    public void setUp() throws Exception {
        mHandlerThread = new HandlerThread("SingleThreadTaskRunnerImplTest thread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    @After
    @TargetApi(Build.VERSION_CODES.LOLLIPOP_MR1)
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
        TaskRunner taskQueue = new SingleThreadTaskRunnerImpl(mHandler, new TaskTraits());
        List<Integer> orderList = new ArrayList<>();
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
        SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);

        SchedulerTestHelpers.preNativeRunUntilIdle(mHandlerThread);
        assertThat(orderList, contains(1, 2, 3));
    }
}
