// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetricsTest;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for histograms emitted from org.chromium.base.library_loader.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class ChromiumAndroidLinkerMetricsTest {
    private static final String PAGE_PREFIX = "/chrome/test/data/android/google.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private int mLoadCount;

    // Provide the next URL to test with. To eliminate a potential source of flakiness each observed
    // URL is unique.
    private String getNextLoadUrl() {
        int i = mLoadCount++;
        return mTestServer.getURL(PAGE_PREFIX + "?q=" + i);
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
    }

    /**
     * Test that *some* library load times are reported.
     */
    @Test
    @LargeTest
    public void testMetricsEmitted() throws Exception {
        PageLoadMetricsTest.PageLoadMetricsTestObserver metricsObserver =
                new PageLoadMetricsTest.PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(metricsObserver, false));

        mActivityTestRule.loadUrl(getNextLoadUrl());

        Assert.assertTrue("First Contentful Paint must be reported",
                metricsObserver.waitForFirstContentfulPaintEvent());
        // Not testing the histogram from non-main process because the values can be stale.

        mActivityTestRule.loadUrl(getNextLoadUrl());
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(metricsObserver));
    }
}
