// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_load_metrics;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for {@link PageLoadMetrics} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(BlinkFeatures.PRERENDER2)
@DisableFeatures(BlinkFeatures.PRERENDER2_MEMORY_CONTROLS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PageLoadMetricsTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final long PAGE_LOAD_METRICS_TIMEOUT_MS = ScalableTimeout.scaleTimeout(20000);
    private static final String PAGE_PREFIX = "/chrome/test/data/android/google.html";

    private EmbeddedTestServer mTestServer;
    private int mLoadCount;
    private WebPageStation mPage;

    // Provide the next URL to test with. To eliminate a potential source of flakiness each observed
    // URL is unique.
    private String getNextLoadUrl() {
        int i = mLoadCount++;
        return mTestServer.getURL(PAGE_PREFIX + "?q=" + i);
    }

    private void addPrerender(String url) throws TimeoutException {
        String script =
                "{\n"
                        + "  const script = document.createElement('script');\n"
                        + "  script.type = 'speculationrules';\n"
                        + "  script.text = `{\n"
                        + "    \"prerender\" : [{\n"
                        + "      \"source\": \"list\",\n"
                        + "      \"urls\": [\""
                        + url
                        + "\"]\n"
                        + "    }]\n"
                        + "  }`;\n"
                        + "  document.head.appendChild(script);\n"
                        + "}";
        mActivityTestRule.runJavaScriptCodeInCurrentTab(script);
    }

    private void activatePrerender(String url) throws TimeoutException {
        // Should not mActivityTestRUle.loadUrl() to activate the prerendered
        // page as such a browser initiated request has different attributes.
        String script = "{ document.location.href='" + url + "' }";
        mActivityTestRule.runJavaScriptCodeInCurrentTab(script);
    }

    @Before
    public void setUp() throws Exception {
        mPage = mActivityTestRule.startOnBlankPage();
        mTestServer = mActivityTestRule.getTestServer();
        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().hasWindowFocus(),
                "Activity should have window focus before test begins");
        CriteriaHelper.pollUiThread(
                () -> !mActivityTestRule.getActivityTab().isLoading(),
                "Tab should not be loading before test begins");
    }

    private void assertMetricsEmitted(PageLoadMetricsTestObserver observer)
            throws InterruptedException {
        Assert.assertTrue(
                "Navigation should be observed (navId=" + observer.getNavigationId() + ")",
                observer.waitForNavigationEvent());
        Assert.assertTrue(
                "First Contentful Paint should be reported (expected navId="
                        + observer.getNavigationId()
                        + ", last observed FCP navId="
                        + observer.getLastObservedFcpNavId()
                        + ", last observed FCP ms="
                        + observer.getLastObservedFcpMs()
                        + ")",
                observer.waitForFirstContentfulPaintEvent());
        Assert.assertTrue(
                "Load event start event should be reported (expected navId="
                        + observer.getNavigationId()
                        + ", last observed load navId="
                        + observer.getLastObservedLoadEventNavId()
                        + ", last observed load ms="
                        + observer.getLastObservedLoadEventMs()
                        + ")",
                observer.waitForLoadEventStartEvent());
    }

    /**
     * Implementation of PageLoadMetrics.Observer for tests that allows to synchronously wait for
     * various page load metrics events. Observes only the first seen navigation, all other
     * navigations are ignored.
     */
    public static class PageLoadMetricsTestObserver implements PageLoadMetrics.Observer {
        private static final long NO_NAVIGATION_ID = -1;

        private final @Nullable WebContents mExpectedWebContents;
        private final CountDownLatch mPrerenderingNavigationLatch = new CountDownLatch(1);
        private final CountDownLatch mActivationLatch = new CountDownLatch(1);
        private final CountDownLatch mFirstContentfulPaintLatch = new CountDownLatch(1);
        private final CountDownLatch mLoadEventStartLatch = new CountDownLatch(1);
        private final CountDownLatch mNavigationLatch = new CountDownLatch(1);
        private volatile long mNavigationId = NO_NAVIGATION_ID;
        private volatile long mPrerenderingId = NO_NAVIGATION_ID;
        private volatile long mLastObservedFcpNavId = NO_NAVIGATION_ID;
        private volatile long mLastObservedFcpMs = -1;
        private volatile long mLastObservedLoadEventNavId = NO_NAVIGATION_ID;
        private volatile long mLastObservedLoadEventMs = -1;

        // Under MPArch (Prerender2), prerendered pages share the host WebContents, so
        // filtering by mExpectedWebContents safely observes both primary and prerendered events.
        public PageLoadMetricsTestObserver(@Nullable WebContents expectedWebContents) {
            mExpectedWebContents = expectedWebContents;
        }

        @Override
        public void onNewNavigation(
                WebContents webContents,
                long navigationId,
                boolean isFirstNavigationInWebContents) {
            if (mExpectedWebContents != null && webContents != mExpectedWebContents) return;

            if (PageLoadMetrics.isPrerendering()) {
                if (mPrerenderingId == NO_NAVIGATION_ID) mPrerenderingId = navigationId;
                mPrerenderingNavigationLatch.countDown();
            } else {
                if (mNavigationId == NO_NAVIGATION_ID) {
                    mNavigationId = navigationId;
                    mNavigationLatch.countDown();
                }
            }
        }

        @Override
        public void onActivation(
                WebContents webContents,
                long prerenderingNavigationId,
                long activatingNavigationId,
                long activationStartMicros) {
            if (mExpectedWebContents != null && webContents != mExpectedWebContents) return;

            Assert.assertEquals(
                    "prerenderingNavigationId should be consistent",
                    mPrerenderingId,
                    prerenderingNavigationId);
            Assert.assertTrue(
                    "prerenderingNavigationId and activatingNavigationId should be different",
                    prerenderingNavigationId != activatingNavigationId);
            Assert.assertFalse(
                    "Activating navigationId should not be registered as a prerendering navigation",
                    PageLoadMetrics.isPrerendering());
            mPrerenderingId = NO_NAVIGATION_ID;
            mNavigationId = activatingNavigationId;

            mActivationLatch.countDown();
        }

        @Override
        public void onFirstContentfulPaint(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long firstContentfulPaintMs) {
            if (mExpectedWebContents != null && webContents != mExpectedWebContents) return;

            mLastObservedFcpNavId = navigationId;
            mLastObservedFcpMs = firstContentfulPaintMs;
            if (mNavigationId != navigationId) return;

            if (firstContentfulPaintMs >= 0) mFirstContentfulPaintLatch.countDown();
        }

        @Override
        public void onLoadEventStart(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long loadEventStartMs) {
            if (mExpectedWebContents != null && webContents != mExpectedWebContents) return;

            mLastObservedLoadEventNavId = navigationId;
            mLastObservedLoadEventMs = loadEventStartMs;
            if (mPrerenderingId != NO_NAVIGATION_ID) {
                if (mPrerenderingId == navigationId) {
                    Assert.assertTrue(
                            "Should be registered as prerendering",
                            PageLoadMetrics.isPrerendering());
                    if (loadEventStartMs >= 0) mLoadEventStartLatch.countDown();
                }
            }
            if (mNavigationId != navigationId) return;

            if (loadEventStartMs >= 0) mLoadEventStartLatch.countDown();
        }

        public boolean waitForNavigationEvent() throws InterruptedException {
            return mNavigationLatch.await(PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public boolean waitForPrerenderingNavigationEvent() throws InterruptedException {
            return mPrerenderingNavigationLatch.await(
                    PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public boolean waitForActivationEvent() throws InterruptedException {
            return mActivationLatch.await(PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public boolean waitForFirstContentfulPaintEvent() throws InterruptedException {
            return mFirstContentfulPaintLatch.await(
                    PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public boolean waitForLoadEventStartEvent() throws InterruptedException {
            return mLoadEventStartLatch.await(PAGE_LOAD_METRICS_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }

        public long getNavigationId() {
            return mNavigationId;
        }

        public boolean hasPrerendering() {
            return mPrerenderingId != NO_NAVIGATION_ID;
        }

        public long getLastObservedFcpNavId() {
            return mLastObservedFcpNavId;
        }

        public long getLastObservedFcpMs() {
            return mLastObservedFcpMs;
        }

        public long getLastObservedLoadEventNavId() {
            return mLastObservedLoadEventNavId;
        }

        public long getLastObservedLoadEventMs() {
            return mLastObservedLoadEventMs;
        }
    }

    @Test
    @SmallTest
    public void testPageLoadMetricEmitted() throws InterruptedException {
        Assert.assertFalse(
                "Tab shouldn't be loading anything before we add observer",
                mActivityTestRule.getActivityTab().isLoading());
        PageLoadMetricsTestObserver metricsObserver =
                new PageLoadMetricsTestObserver(mActivityTestRule.getWebContents());
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        PageLoadMetrics.addObserver(
                                metricsObserver, /* supportPrerendering= */ false));

        try {
            mActivityTestRule.loadUrl(getNextLoadUrl());
            assertMetricsEmitted(metricsObserver);
            Assert.assertFalse("Should not have prerendering", metricsObserver.hasPrerendering());
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> PageLoadMetrics.removeObserver(metricsObserver));
        }
    }

    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/481445205
    public void testPageLoadMetricNavigationIdSetCorrectly() throws InterruptedException {
        PageLoadMetricsTestObserver metricsObserver =
                new PageLoadMetricsTestObserver(mActivityTestRule.getWebContents());
        PageLoadMetricsTestObserver metricsObserver2 =
                new PageLoadMetricsTestObserver(mActivityTestRule.getWebContents());
        try {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            PageLoadMetrics.addObserver(
                                    metricsObserver, /* supportPrerendering= */ false));
            mActivityTestRule.loadUrl(getNextLoadUrl());
            assertMetricsEmitted(metricsObserver);

            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            PageLoadMetrics.addObserver(
                                    metricsObserver2, /* supportPrerendering= */ false));
            mActivityTestRule.loadUrl(getNextLoadUrl());
            assertMetricsEmitted(metricsObserver2);

            Assert.assertNotEquals(
                    "Subsequent navigations should have different navigation ids",
                    metricsObserver.getNavigationId(),
                    metricsObserver2.getNavigationId());
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        PageLoadMetrics.removeObserver(metricsObserver);
                        PageLoadMetrics.removeObserver(metricsObserver2);
                    });
        }
    }

    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/481445205
    public void testPageLoadMetricForPrerendering() throws Exception {
        Assert.assertFalse(
                "Tab shouldn't be loading anything before we add observer",
                mActivityTestRule.getActivityTab().isLoading());
        // Add two observers, one doesn't support prerendering, and the other is does.
        PageLoadMetricsTestObserver metricsObserver =
                new PageLoadMetricsTestObserver(mActivityTestRule.getWebContents());
        PageLoadMetricsTestObserver prerenderingSupportMetricsObserver =
                new PageLoadMetricsTestObserver(mActivityTestRule.getWebContents());
        try {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            PageLoadMetrics.addObserver(
                                    metricsObserver, /* supportPrerendering= */ false));
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            PageLoadMetrics.addObserver(
                                    prerenderingSupportMetricsObserver,
                                    /* supportPrerendering= */ true));

            mActivityTestRule.loadUrl(getNextLoadUrl());
            // Both observers should recognize primary page's metrics.
            assertMetricsEmitted(metricsObserver);
            assertMetricsEmitted(prerenderingSupportMetricsObserver);
            Assert.assertFalse("Should not have prerendering", metricsObserver.hasPrerendering());
            Assert.assertFalse(
                    "Should not have prerendering yet",
                    prerenderingSupportMetricsObserver.hasPrerendering());

            String prerenderingUrl = getNextLoadUrl();
            addPrerender(prerenderingUrl);
            Assert.assertTrue(
                    "Prerendering navigation should be observed",
                    prerenderingSupportMetricsObserver.waitForPrerenderingNavigationEvent());
            Assert.assertFalse(
                    "Observers that don't support prerendering should not recognize prerendering",
                    metricsObserver.hasPrerendering());
            Assert.assertTrue(
                    "Observers that support prerendering should recognize prerendering",
                    prerenderingSupportMetricsObserver.hasPrerendering());
            Assert.assertTrue(
                    "Observers that support prerendering should recognize prerendering load event",
                    prerenderingSupportMetricsObserver.waitForLoadEventStartEvent());

            // Activate the prerendered page.
            activatePrerender(prerenderingUrl);
            Assert.assertTrue(
                    "Observers that support prerendering should observe activation event",
                    prerenderingSupportMetricsObserver.waitForActivationEvent());
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        PageLoadMetrics.removeObserver(metricsObserver);
                        PageLoadMetrics.removeObserver(prerenderingSupportMetricsObserver);
                    });
        }
    }
}
