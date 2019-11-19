// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.os.Looper;
import android.support.test.filters.MediumTest;

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
                mActivityTestRule.createAwBrowserContext();
                mActivityTestRule.startBrowserProcess();
            } catch (Exception e) {
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

    @Before
    public void setUp() {
        mDefaultUncaughtExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
        mBackgroundThread = new BackgroundThread("background");
        mBackgroundThread.start();
        // Once the background thread looper exists, it has been
        // designated as the main thread.
        mBackgroundThread.getLooper();
    }

    @After
    public void tearDown() throws InterruptedException {
        Looper backgroundThreadLooper = mBackgroundThread.getLooper();
        if (backgroundThreadLooper != null) {
            backgroundThreadLooper.quitSafely();
        }
        mBackgroundThread.join();
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
        Assert.assertTrue(latch.await(WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
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
        Assert.assertTrue(latch.await(WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
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

        Assert.assertTrue(latch.await(WAIT_TIMEOUT_MS, java.util.concurrent.TimeUnit.MILLISECONDS));
    }
};
