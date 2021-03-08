// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link PageLoadMetrics}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PageLoadMetricsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int PAGE_LOAD_METRICS_TIMEOUT_MS = 6000;
    private static final String PAGE_PREFIX = "/chrome/test/data/android/google.html";

    private EmbeddedTestServer mTestServer;
    private int mLoadCount;

    // Provide the next URL to test with. To eliminate a potential source of flakiness each observed
    // URL is unique.
    private String getNextLoadUrl() {
        int i = mLoadCount++;
        return mTestServer.getURL(PAGE_PREFIX + "?q=" + i);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
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

        @Override
        public void onNewNavigation(WebContents webContents, long navigationId,
                boolean isFirstNavigationInWebContents) {
            if (mNavigationId == NO_NAVIGATION_ID) mNavigationId = navigationId;
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
    }

    @Test
    @SmallTest
    public void testPageLoadMetricEmitted() throws InterruptedException {
        Assert.assertFalse("Tab shouldn't be loading anything before we add observer",
                mActivityTestRule.getActivity().getActivityTab().isLoading());
        PageLoadMetricsTestObserver metricsObserver = new PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver));

        mActivityTestRule.loadUrl(getNextLoadUrl());
        assertMetricsEmitted(metricsObserver);

        mActivityTestRule.loadUrl(getNextLoadUrl());
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver));
    }

    @Test
    @SmallTest
    public void testPageLoadMetricNavigationIdSetCorrectly() throws InterruptedException {
        PageLoadMetricsTestObserver metricsObserver = new PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver));
        mActivityTestRule.loadUrl(getNextLoadUrl());
        assertMetricsEmitted(metricsObserver);

        PageLoadMetricsTestObserver metricsObserver2 = new PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver2));
        mActivityTestRule.loadUrl(getNextLoadUrl());
        assertMetricsEmitted(metricsObserver2);

        Assert.assertNotEquals("Subsequent navigations should have different navigation ids",
                metricsObserver.getNavigationId(), metricsObserver2.getNavigationId());

        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver));
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver2));
    }
}
