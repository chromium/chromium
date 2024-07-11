// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;

import androidx.core.content.ContextCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.display.DisplayAndroid;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests ScreenshotMonitor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ScreenshotMonitorTest {
    private static final String TAG = "ScreenshotTest";
    private static final Uri TEST_URI = Uri.parse("content://media/external/images/media/101");

    private ScreenshotMonitorImpl mTestScreenshotMonitor;
    private TestScreenshotMonitorDelegate mTestScreenshotMonitorDelegate;
    private ContentObserver mContentObserver;

    private MockContentResolver mMockContentResolver = new MockContentResolver();

    @Mock private DisplayAndroid mDisplayAndroid;

    static class TestScreenshotMonitorDelegate implements ScreenshotMonitorDelegate {
        // This is modified on the UI thread and accessed on the test thread.
        public final AtomicInteger screenshotShowUiCount = new AtomicInteger();

        @Override
        public void onScreenshotTaken() {
            Assert.assertTrue(ThreadUtils.runningOnUiThread());
            screenshotShowUiCount.getAndIncrement();
        }
    }

    private static class TestContext extends ContextWrapper {
        public TestContext(Context base) {
            super(base);
        }

        @Override
        public int checkPermission(String permission, int pid, int uid) {
            return PackageManager.PERMISSION_GRANTED;
        }
    }

    @Before
    public void setUp() {
        // Replaces the application context with a test implementation which will return true for
        // permission requests. This is needed for the permission check in
        // ScreenshotMonitorImpl#doesChangeLookLikeScreenshot.
        Context context = new TestContext(ApplicationProvider.getApplicationContext());
        ContextUtils.initApplicationContextForTests(context);
        Assume.assumeTrue(
                ContextCompat.checkSelfPermission(
                                ContextUtils.getApplicationContext(),
                                MimeTypeUtils.getPermissionNameForMimeType(
                                        MimeTypeUtils.Type.IMAGE))
                        == PackageManager.PERMISSION_GRANTED);

        MockitoAnnotations.initMocks(this);
        mTestScreenshotMonitorDelegate = new TestScreenshotMonitorDelegate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestScreenshotMonitor =
                            new ScreenshotMonitorImpl(
                                    mTestScreenshotMonitorDelegate,
                                    null,
                                    mMockContentResolver,
                                    mDisplayAndroid);
                    mContentObserver = mTestScreenshotMonitor.getContentObserver();
                });
    }

    private void mockValidContentResolver(String path, String width, String height) {
        final Cursor cursor = Mockito.mock(Cursor.class);
        Mockito.doReturn(true).when(cursor).moveToNext();

        Mockito.doReturn(1)
                .when(cursor)
                .getColumnIndexOrThrow(Mockito.eq(MediaStore.MediaColumns.DATA));
        Mockito.doReturn(2)
                .when(cursor)
                .getColumnIndexOrThrow(Mockito.eq(MediaStore.MediaColumns.WIDTH));
        Mockito.doReturn(3)
                .when(cursor)
                .getColumnIndexOrThrow(Mockito.eq(MediaStore.MediaColumns.HEIGHT));
        Mockito.doReturn(path).when(cursor).getString(Mockito.eq(1));
        Mockito.doReturn(width).when(cursor).getString(Mockito.eq(2));
        Mockito.doReturn(height).when(cursor).getString(Mockito.eq(3));

        mMockContentResolver.addProvider(
                "media",
                new MockContentProvider() {
                    @Override
                    public Cursor query(
                            Uri uri,
                            String[] projection,
                            String selection,
                            String[] selectionArgs,
                            String sortOrder) {
                        return cursor;
                    }
                });
    }

    private void mockDisplay(int width, int height) {
        Mockito.doReturn(width).when(mDisplayAndroid).getDisplayWidth();
        Mockito.doReturn(height).when(mDisplayAndroid).getDisplayHeight();
    }

    private void mockValidScreenshot() {}

    /**
     * Verify that if monitoring starts, the delegate should be called. Also verify that the inner
     * TestFileObserver monitors as expected.
     */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testDelegateCalledOnEvent() {
        mockDisplay(50, 100);
        mockValidContentResolver("Screenshot", "50", "100");

        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(1);

        stopMonitoringOnUiThreadBlocking();
    }

    /** Verify that the delegate is called after a restart. */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testRestartShouldTriggerDelegate() {
        mockDisplay(50, 100);
        mockValidContentResolver("Screenshot", "50", "100");

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

    /** Verify that if monitoring stops, the delegate should not be called. */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testStopMonitoringShouldNotTriggerDelegate() {
        mockDisplay(50, 100);
        mockValidContentResolver("Screenshot", "50", "100");

        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        stopMonitoringOnUiThreadBlocking();

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(0);
    }

    /** Verify that if monitoring is never started, the delegate should not be called. */
    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testNoMonitoringShouldNotTriggerDelegate() {
        mockDisplay(50, 100);
        mockValidContentResolver("Screenshot", "50", "100");

        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(0);
    }

    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testRotatedContent() {
        mockDisplay(100, 50);
        mockValidContentResolver("Screenshot", "50", "100");

        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(1);
    }

    @Test
    @SmallTest
    @Feature({"FeatureEngagement", "Screenshot"})
    public void testInvalidSize() {
        mockDisplay(150, 150);
        mockValidContentResolver("Screenshot", "50", "100");

        startMonitoringOnUiThreadBlocking();
        Assert.assertEquals(0, mTestScreenshotMonitorDelegate.screenshotShowUiCount.get());

        mContentObserver.onChange(true, TEST_URI);
        assertScreenshotShowUiCountOnUiThreadBlocking(0);
    }

    // This ensures that the UI thread finishes executing startMonitoring.
    private void startMonitoringOnUiThreadBlocking() {
        final Semaphore semaphore = new Semaphore(0);

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
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

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
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

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
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
