// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link PageLoadMetrics}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PageLoadMetricsTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final int PAGE_LOAD_METRICS_TIMEOUT_MS = 3000;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";

    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void assertMetricsEmitted(PageLoadMetricsTestObserver observer)
            throws InterruptedException {
        Assert.assertTrue("First Contentful Paint should be reported",
                observer.waitForFirstContentfulPaintEvent());
        Assert.assertTrue(
                "Load event start event should be reported", observer.waitForLoadEventStartEvent());
    }

    /**
     * Implementation of PageLoadMetrics.Observer for tests that allows to synchronously wait for
     * various page load metrics events. Observes only the first seen navigation, all other
     * navigations are ignored.
     */
    static class PageLoadMetricsTestObserver implements PageLoadMetrics.Observer {
        private static final long NO_NAVIGATION_ID = -1;

        private final CountDownLatch mFirstContentfulPaintLatch = new CountDownLatch(1);
        private final CountDownLatch mLoadEventStartLatch = new CountDownLatch(1);
        private long mNavigationId = NO_NAVIGATION_ID;
        private Boolean mIsFirstNavigationInWebContents;

        @Override
        public void onNewNavigation(WebContents webContents, long navigationId,
                boolean isFirstNavigationInWebContents) {
            if (mNavigationId == NO_NAVIGATION_ID) mNavigationId = navigationId;
            mIsFirstNavigationInWebContents = isFirstNavigationInWebContents;
        }

        @Override
        public void onFirstContentfulPaint(WebContents webContents, long navigationId,
                long navigationStartTick, long firstContentfulPaintMs) {
            if (mNavigationId != navigationId) return;

            if (firstContentfulPaintMs > 0) mFirstContentfulPaintLatch.countDown();
        }

        @Override
        public void onLoadEventStart(WebContents webContents, long navigationId,
                long navigationStartTick, long loadEventStartMs) {
            if (mNavigationId != navigationId) return;

            if (loadEventStartMs > 0) mLoadEventStartLatch.countDown();
        }

        // Wait methods below assume that the navigation either has already started or it will never
        // start.
        public boolean waitForFirstContentfulPaintEvent() throws InterruptedException {
            // The event will not occur if there is no navigation to observe, so we can exit
            // earlier.
            if (mNavigationId == NO_NAVIGATION_ID) return false;

            return mFirstContentfulPaintLatch.await(
                    PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public boolean waitForLoadEventStartEvent() throws InterruptedException {
            // The event will not occur if there is no navigation to observe, so we can exit
            // earlier.
            if (mNavigationId == NO_NAVIGATION_ID) return false;

            return mLoadEventStartLatch.await(PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public long getNavigationId() {
            return mNavigationId;
        }

        public Boolean getIsFirstNavigationInWebContents() {
            return mIsFirstNavigationInWebContents;
        }
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/983804")
    public void testPageLoadMetricEmitted() throws InterruptedException {
        Assert.assertFalse("Tab shouldn't be loading anything before we add observer",
                mActivityTestRule.getActivity().getActivityTab().isLoading());
        PageLoadMetricsTestObserver metricsObserver = new PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver));

        Assert.assertNull(metricsObserver.getIsFirstNavigationInWebContents());

        mActivityTestRule.loadUrl(mTestPage);
        Assert.assertTrue(metricsObserver.getIsFirstNavigationInWebContents().booleanValue());
        assertMetricsEmitted(metricsObserver);

        mActivityTestRule.loadUrl(mTestPage);
        Assert.assertFalse(metricsObserver.getIsFirstNavigationInWebContents().booleanValue());

        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver));
    }

    @Test
    @SmallTest
    @FlakyTest(message = "crbug.com/986025")
    public void testPageLoadMetricNavigationIdSetCorrectly() throws InterruptedException {
        PageLoadMetricsTestObserver metricsObserver = new PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver));
        mActivityTestRule.loadUrl(mTestPage);
        assertMetricsEmitted(metricsObserver);

        PageLoadMetricsTestObserver metricsObserver2 = new PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver2));
        mActivityTestRule.loadUrl(mTestPage2);
        assertMetricsEmitted(metricsObserver2);

        Assert.assertNotEquals("Subsequent navigations should have different navigation ids",
                metricsObserver.getNavigationId(), metricsObserver2.getNavigationId());

        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver));
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver2));
    }
}
