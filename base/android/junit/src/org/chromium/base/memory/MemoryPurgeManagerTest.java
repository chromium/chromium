// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import android.os.Looper;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ApplicationState;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;

/** Tests for MemoryPurgeManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MemoryPurgeManagerTest {
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();
    private Callable<Integer> mGetCount =
            () -> {
                return RecordHistogram.getHistogramTotalCountForTesting(
                        MemoryPurgeManager.BACKGROUND_DURATION_HISTOGRAM_NAME);
            };

    private static class MemoryPurgeManagerForTest extends MemoryPurgeManager {
        public MemoryPurgeManagerForTest(int initialState) {
            super();
            mApplicationState = initialState;
        }

        @Override
        public void onApplicationStateChange(int state) {
            mApplicationState = state;
            super.onApplicationStateChange(state);
        }

        @Override
        protected void notifyMemoryPressure() {
            mMemoryPressureNotifiedCount += 1;
        }

        @Override
        protected int getApplicationState() {
            return mApplicationState;
        }

        public int mMemoryPressureNotifiedCount;
        public int mApplicationState = ApplicationState.UNKNOWN;
    }

    @Before
    public void setUp() {
        // Explicitly set main thread as UiThread. Other places rely on that.
        ThreadUtils.setUiThread(Looper.getMainLooper());

        // Pause main thread to get control over when tasks are run (see runUiThreadFor()).
        ShadowLooper.pauseMainLooper();
    }

    @Test
    @SmallTest
    public void testSimple() throws Exception {
        int count = mGetCount.call();
        var manager = new MemoryPurgeManagerForTest(ApplicationState.HAS_RUNNING_ACTIVITIES);
        manager.start();

        // No notification when initial state has activities.
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);
        Assert.assertEquals(count, (int) mGetCount.call());

        // Notify after a delay.
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount); // Should be delayed.
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);

        // Only one notification.
        manager.onApplicationStateChange(ApplicationState.HAS_DESTROYED_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);

        // Started in foreground, went to background once, and came back.
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertEquals(count + 1, (int) mGetCount.call());
    }

    @Test
    @SmallTest
    public void testInitializedOnceInBackground() throws Exception {
        int count = mGetCount.call();
        var manager = new MemoryPurgeManagerForTest(ApplicationState.HAS_STOPPED_ACTIVITIES);
        manager.start();
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);

        // Started in background, no histogram recording when coming to foreground.
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertEquals(count, (int) mGetCount.call());
    }

    @Test
    @SmallTest
    public void testDontTriggerForProcessesWithNoActivities() {
        var manager = new MemoryPurgeManagerForTest(ApplicationState.HAS_DESTROYED_ACTIVITIES);
        manager.start();

        // Don't purge if the process never hosted any activity.
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);

        // Starts when we cycle through foreground and background.
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);
    }

    @Test
    @SmallTest
    public void testMultiple() throws Exception {
        int count = mGetCount.call();
        var manager = new MemoryPurgeManagerForTest(ApplicationState.HAS_RUNNING_ACTIVITIES);
        manager.start();

        // Notify after a delay.
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);

        // Back to foreground, no notification.
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);
        Assert.assertEquals(count + 1, (int) mGetCount.call());

        // Background again, notify
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        Assert.assertEquals(2, manager.mMemoryPressureNotifiedCount);
        Assert.assertEquals(count + 1, (int) mGetCount.call());

        // Foreground again, record the histogram.
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        Assert.assertEquals(count + 2, (int) mGetCount.call());
    }

    @Test
    @SmallTest
    public void testNoEnoughTimeInBackground() {
        var manager = new MemoryPurgeManagerForTest(ApplicationState.HAS_RUNNING_ACTIVITIES);
        manager.start();

        // Background, then foregound inside the delay period.
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS / 2);
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS);
        // Went back to foreground, do nothing.
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);

        // Starts the new task
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        // After some time, foregroung/background cycle.
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS / 2);
        manager.onApplicationStateChange(ApplicationState.HAS_RUNNING_ACTIVITIES);
        manager.onApplicationStateChange(ApplicationState.HAS_STOPPED_ACTIVITIES);
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS / 2);
        // Not enough time in background.
        Assert.assertEquals(0, manager.mMemoryPressureNotifiedCount);
        // But the task got rescheduled.
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS / 2);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);

        // No new notification.
        runUiThreadFor(MemoryPurgeManager.PURGE_DELAY_MS * 2);
        Assert.assertEquals(1, manager.mMemoryPressureNotifiedCount);
    }

    private void runUiThreadFor(long delayMs) {
        mFakeTimeTestRule.advanceMillis(delayMs);
        ShadowLooper.idleMainLooper(delayMs, TimeUnit.MILLISECONDS);
    }
}
