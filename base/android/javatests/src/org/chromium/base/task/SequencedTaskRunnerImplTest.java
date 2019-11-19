// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.support.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;

import java.util.ArrayList;
import java.util.List;

/**
 * Test class for {@link SequencedTaskRunnerImpl}.
 *
 * Note due to layering concerns we can't test post native functionality in a
 * base javatest. Instead see:
 * content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * NativePostTaskTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SequencedTaskRunnerImplTest {
    @Test
    @SmallTest
    public void testPreNativeTasksRunInOrder() {
        TaskRunner taskQueue = new SequencedTaskRunnerImpl(TaskTraits.USER_BLOCKING);
        List<Integer> orderList = new ArrayList<>();
        try {
            SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 1);
            SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 2);
            SchedulerTestHelpers.postRecordOrderTask(taskQueue, orderList, 3);
            SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);
        } finally {
            taskQueue.destroy();
        }
        assertThat(orderList, contains(1, 2, 3));
    }
}
