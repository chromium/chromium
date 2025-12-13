// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.TraceEvent;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;

@RunWith(BaseRobolectricTestRunner.class)
public class LocationTest {
    @After
    public void tearDown() {
        TraceEvent.setEnabled(false);
    }

    /**
     * Test that Location.from() returns null if Chome is not being traced as an optimization to
     * reduce throwaway work when posting tasks.
     */
    @Test
    public void testLocationFromIsNullIfTracingDisabled() {
        final String file = "test.java";
        final String func = "testMethod";
        final int line = 10101010;

        TraceEvent.setEnabled(false);
        Assert.assertNull(Location.from(file, func, line));

        TraceEvent.setEnabled(true);
        Assert.assertEquals(Location.from(file, func, line), new Location(file, func, line));
    }

    /**
     * Test if the location rewriter successfully replaced calls to TaskRunner#postDelayedTask and
     * TaskRunnerImpl#postDelayedTask with their location-aware overloads.
     */
    @Test
    public void testLocationRewrittenForTaskRunner() {
        final Runnable doNothing = () -> {};
        final String file = "org/chromium/base/task/LocationTest.java";
        final String func = "testLocationRewrittenForTaskRunner";

        FakeTaskRunnerImplNatives natives = new FakeTaskRunnerImplNatives();
        TaskRunnerImplJni.setInstanceForTesting(natives);
        Assert.assertNull(natives.lastPostedTaskLocation());

        TaskRunnerImpl taskRunnerImpl = new TaskRunnerImpl(TaskTraits.THREAD_POOL_TRAITS_START);
        taskRunnerImpl.initNativeTaskRunner();

        // Java should only propagate location to native when tracing is enabled.
        TraceEvent.setEnabled(false);

        taskRunnerImpl.postDelayedTask(doNothing, 0);
        Assert.assertNull(natives.lastPostedTaskLocation());
        Assert.assertEquals(1, natives.postedTaskCount());

        TraceEvent.setEnabled(true);

        taskRunnerImpl.postDelayedTask(doNothing, 0);
        Assert.assertEquals(new Location(file, func, 66), natives.lastPostedTaskLocation());
        Assert.assertEquals(2, natives.postedTaskCount());

        ((TaskRunner) taskRunnerImpl).postDelayedTask(doNothing, 0);
        Assert.assertEquals(new Location(file, func, 70), natives.lastPostedTaskLocation());
        Assert.assertEquals(3, natives.postedTaskCount());
    }

    private static class FakeTaskRunnerImplNatives implements TaskRunnerImpl.Natives {
        private @Nullable Location mLastPostedTaskLocation;
        private int mPostedTaskCount;

        public FakeTaskRunnerImplNatives() {
            mLastPostedTaskLocation = null;
            mPostedTaskCount = 0;
        }

        public @Nullable Location lastPostedTaskLocation() {
            return mLastPostedTaskLocation;
        }

        public int postedTaskCount() {
            return mPostedTaskCount;
        }

        private void didPostTask(@Nullable Location location) {
            mLastPostedTaskLocation = location;
            mPostedTaskCount++;
        }

        @Override
        public long init(int taskRunnerType, int taskTraits) {
            return 1;
        }

        @Override
        public void destroy(long nativeTaskRunnerAndroid) {}

        @Override
        public void postDelayedTask(long nativeTaskRunnerAndroid, long delay, int taskIndex) {
            didPostTask(null);
        }

        @Override
        public void postDelayedTaskWithLocation(
                long nativeTaskRunnerAndroid,
                long delay,
                int taskIndex,
                String fileName,
                String functionName,
                int lineNumber) {
            didPostTask(new Location(fileName, functionName, lineNumber));
        }
    }
}
