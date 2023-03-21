// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import android.os.Looper;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitor;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.crash.AwCrashReporterClient;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

import java.lang.reflect.Field;
import java.util.concurrent.CountDownLatch;

/**
 * Test suite for actions that should cause java exceptions to be
 * propagated to the embedding application.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwUncaughtExceptionTest {
    // Initialization of WebView is delayed until a background thread
    // is started. This gives us the chance to process the uncaught
    // exception off the UI thread. An uncaught exception on the UI
    // thread appears to cause the test to fail to exit.
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        @Override
        public boolean needsAwBrowserContextCreated() {
            return false;
        }
        @Override
        public boolean needsBrowserProcessStarted() {
            return false;
        }
        @Override
        public boolean needsAwContentsCleanup() {
            // State of VM might be hosed after throwing and not catching exceptions.
            // Do not assume it is safe to destroy AwContents by posting to the UI thread.
            // Instead explicitly destroy any AwContents created in this test.
            return false;
        }
    };

    private class BackgroundThread extends Thread {
        private Looper mLooper;

        BackgroundThread(String name) {
            super(name);
        }

        @Override
        public void run() {
            Looper.prepare();
            synchronized (this) {
                mLooper = Looper.myLooper();
                ThreadUtils.setUiThread(mLooper);
                notifyAll();
            }
            try {
                Looper.loop();
            } finally {
            }
        }

        public Looper getLooper() {
            if (!isAlive()) return null;
            synchronized (this) {
                while (isAlive() && mLooper == null) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                    }
                }
            }
            return mLooper;
        }
    };

    private BackgroundThread mBackgroundThread;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private Thread.UncaughtExceptionHandler mDefaultUncaughtExceptionHandler;

    // Since this test overrides the UI thread, Android's ActivityLifecycleMonitor assertions fail
    // as our UI thread isn't the Main Looper thread, so we have to disable them.
    private void disableLifecycleThreadAssertion() throws Exception {
        ActivityLifecycleMonitor monitor = ActivityLifecycleMonitorRegistry.getInstance();
        Field declawThreadCheck = monitor.getClass().getDeclaredField("declawThreadCheck");
        declawThreadCheck.setAccessible(true);
        declawThreadCheck.set(monitor, true);
    }

    @Before
    public void setUp() throws Exception {
        disableLifecycleThreadAssertion();
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mDefaultUncaughtExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
        mBackgroundThread = new BackgroundThread("background");
        mBackgroundThread.start();
        // Once the background thread looper exists, it has been
        // designated as the main thread.
        mBackgroundThread.getLooper();
        mActivityTestRule.createAwBrowserContext();
        mActivityTestRule.startBrowserProcess();
    }

    @After
    public void tearDown() throws InterruptedException {
        Looper backgroundThreadLooper = mBackgroundThread.getLooper();
        if (backgroundThreadLooper != null) {
            backgroundThreadLooper.quitSafely();
        }
        mBackgroundThread.join();
        ThreadUtils.clearUiThreadForTesting();
        Thread.setDefaultUncaughtExceptionHandler(mDefaultUncaughtExceptionHandler);
    }

    private void expectUncaughtException(Thread onThread, Class<? extends Exception> exceptionClass,
            String message, boolean reportable, Runnable onException) {
        Thread.setDefaultUncaughtExceptionHandler((thread, exception) -> {
            if ((onThread == null || onThread.equals(thread))
                    && (exceptionClass == null || exceptionClass.isInstance(exception))
                    && (message == null || exception.getMessage().equals(message))) {
                Assert.assertEquals(
                        reportable, AwCrashReporterClient.stackTraceContainsWebViewCode(exception));
                onException.run();
            } else {
                mDefaultUncaughtExceptionHandler.uncaughtException(thread, exception);
            }
        });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testUncaughtReportedException() throws InterruptedException {
        final CountDownLatch latch = new CountDownLatch(1);
        final String msg = "dies.";

        expectUncaughtException(mBackgroundThread, RuntimeException.class, msg,
                true /* reportable */, () -> { latch.countDown(); });

        ThreadUtils.postOnUiThread(() -> {
            RuntimeException exception = new RuntimeException(msg);
            exception.setStackTrace(new StackTraceElement[] {
                    new StackTraceElement("android.webkit.WebView", "loadUrl", "<none>", 0)});
            throw exception;
        });
        Assert.assertTrue(
                latch.await(SCALED_WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testUncaughtUnreportedException() throws InterruptedException {
        final CountDownLatch latch = new CountDownLatch(1);
        final String msg = "dies.";

        expectUncaughtException(mBackgroundThread, RuntimeException.class, msg,
                false /* reportable */, () -> { latch.countDown(); });

        ThreadUtils.postOnUiThread(() -> {
            RuntimeException exception = new RuntimeException(msg);
            exception.setStackTrace(new StackTraceElement[] {
                    new StackTraceElement("java.lang.Object", "equals", "<none>", 0)});
            throw exception;
        });
        Assert.assertTrue(
                latch.await(SCALED_WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testShouldOverrideUrlLoading() throws InterruptedException {
        final CountDownLatch latch = new CountDownLatch(1);
        final String msg = "dies.";

        expectUncaughtException(mBackgroundThread, RuntimeException.class, msg,
                true /* reportable */, () -> { latch.countDown(); });

        ThreadUtils.postOnUiThread(() -> {
            mContentsClient = new TestAwContentsClient() {
                @Override
                public boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
                    mAwContents.destroyNatives();
                    throw new RuntimeException(msg);
                }
            };
            mTestContainerView =
                    mActivityTestRule.createDetachedAwTestContainerView(mContentsClient);
            mAwContents = mTestContainerView.getAwContents();
            mAwContents.getSettings().setJavaScriptEnabled(true);
            mAwContents.loadUrl(
                    "data:text/html,<script>window.location='https://www.google.com';</script>");
        });

        Assert.assertTrue(
                latch.await(SCALED_WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
    }
};
