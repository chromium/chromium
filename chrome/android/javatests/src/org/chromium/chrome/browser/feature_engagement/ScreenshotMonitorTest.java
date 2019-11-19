// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

import android.database.ContentObserver;
import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
/**
 * Tests ScreenshotMonitor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ScreenshotMonitorTest {
    private static final String FILENAME = "image.jpeg";
    private static final String TAG = "ScreenshotTest";
    private static final Uri TEST_URI = Uri.parse("content://media/external/images/media/101");

    private ScreenshotMonitor mTestScreenshotMonitor;
    private TestScreenshotMonitorDelegate mTestScreenshotMonitorDelegate;
    private ContentObserver mContentObserver;

    static class TestScreenshotMonitorDelegate implements ScreenshotMonitorDelegate {
        // This is modified on the UI thread and accessed on the test thread.
        public final AtomicInteger screenshotShowUiCount = new AtomicInteger();

        @Override
        public void onScreenshotTaken() {
            Assert.assertTrue(ThreadUtils.runningOnUiThread());
            screenshotShowUiCount.getAndIncrement();
        }
    }

    @Before
    public void setUp() {
        mTestScreenshotMonitorDelegate = new TestScreenshotMonitorDelegate();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestScreenshotMonitor = new ScreenshotMonitor(mTestScreenshotMonitorDelegate);
            mContentObserver = mTestScreenshotMonitor.getContentObserver();
            mTestScreenshotMonitor.setSkipOsCallsForUnitTesting();
        });
    }

    /**
     * Verify that if monitoring starts, the delegate should be called. Also verify that the
     * inner TestFileObserver monitors as expected.
     */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testDelegateCalledOnEvent() {
        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(1);

        stopMonitoringOnUiThreadBlocking();
    }

    /**
     * Verify that the delegate is called after a restart.
     */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testRestartShouldTriggerDelegate() {
        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(1);

        stopMonitoringOnUiThreadBlocking();

        // Restart and call onEvent a second time
        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(1, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(2);
    }

    /**
     * Verify that if monitoring stops, the delegate should not be called.
     */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testStopMonitoringShouldNotTriggerDelegate() {
        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        stopMonitoringOnUiThreadBlocking();

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(0);
    }

    /**
     * Verify that if monitoring is never started, the delegate should not be called.
     */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testNoMonitoringShouldNotTriggerDelegate() {
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(0);
    }

    // This ensures that the UI thread finishes executing startMonitoring.
    private void startMonitoringOnUiThreadBlocking() {
        final Semaphore semaphore = new Semaphore(0);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mTestScreenshotMonitor.startMonitoring();
                semaphore.release();
            }
        });
        try {
            Assert.assertTrue(semaphore.tryAcquire(10, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Log.e(TAG, "Cannot acquire semaphore");
        }
    }

    // This ensures that the UI thread finishes executing stopMonitoring.
    private void stopMonitoringOnUiThreadBlocking() {
        final Semaphore semaphore = new Semaphore(0);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mTestScreenshotMonitor.stopMonitoring();
                semaphore.release();
            }
        });
        try {
            Assert.assertTrue(semaphore.tryAcquire(10, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Log.e(TAG, "Cannot acquire semaphore");
        }
    }

    // This ensures that after UI thread finishes all tasks, screenshotShowUiCount equals
    // expectedCount.
    private void assertScreenshotShowUiCountOnUiThreadBlocking(int expectedCount) {
        final Semaphore semaphore = new Semaphore(0);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                semaphore.release();
            }
        });
        try {
            Assert.assertTrue(semaphore.tryAcquire(10, TimeUnit.SECONDS));
        } catch (InterruptedException e) {
            Log.e(TAG, "Cannot acquire semaphore");
        }
        Assert.assertEquals(
                expectedCount, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());
    }
}
