// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebApkActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.webapps.WebApkActivityTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for startup timing histograms.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class StartupLoadingMetricsTest {
    private static final String TAG = "StartupLoadingTest";
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";
    private static final String ERROR_PAGE = "/close-socket";
    private static final String SLOW_PAGE = "/slow?2";
    private static final String FIRST_COMMIT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstNavigationCommit";
    private static final String FIRST_CONTENTFUL_PAINT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstContentfulPaint";
    private static final String FIRST_VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstVisibleContent";
    private static final String VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToVisibleContent";

    private static final String TABBED_SUFFIX = ChromeTabbedActivity.STARTUP_UMA_HISTOGRAM_SUFFIX;
    private static final String WEBAPK_SUFFIX =
            WebApkActivityLifecycleUmaTracker.STARTUP_UMA_HISTOGRAM_SUFFIX;

    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public WebApkActivityTestRule mWebApkActivityTestRule = new WebApkActivityTestRule();

    private String mTestPage;
    private String mTestPage2;
    private String mErrorPage;
    private String mSlowPage;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        mErrorPage = mTestServer.getURL(ERROR_PAGE);
        mSlowPage = mTestServer.getURL(SLOW_PAGE);
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private interface CheckedRunnable { void run() throws Exception; }

    private void runAndWaitForPageLoadMetricsRecorded(CheckedRunnable runnable) throws Exception {
        PageLoadMetricsTest.PageLoadMetricsTestObserver testObserver =
                new PageLoadMetricsTest.PageLoadMetricsTestObserver();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.addObserver(testObserver));
        runnable.run();
        // First Contentful Paint may be recorded asynchronously after a page load is finished, we
        // have to wait the event to occur.
        testObserver.waitForFirstContentfulPaintEvent();
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PageLoadMetrics.removeObserver(testObserver));
    }

    private void loadUrlAndWaitForPageLoadMetricsRecorded(
            ChromeActivityTestRule chromeActivityTestRule, String url) throws Exception {
        runAndWaitForPageLoadMetricsRecorded(() -> chromeActivityTestRule.loadUrl(url));
    }

    private void assertHistogramsRecorded(int expectedCount, String histogramSuffix) {
        Assert.assertEquals(expectedCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + histogramSuffix));
        Assert.assertEquals(expectedCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_CONTENTFUL_PAINT_HISTOGRAM + histogramSuffix));
        if (histogramSuffix.equals(TABBED_SUFFIX)) {
            Assert.assertEquals(expectedCount,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            FIRST_VISIBLE_CONTENT_HISTOGRAM));
            Assert.assertEquals(expectedCount,
                    RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM));
        }
    }

    /**
     * Tests that the startup loading histograms are recorded only once on startup.
     */
    @Test
    @LargeTest
    public void testWebApkStartRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mWebApkActivityTestRule.startWebApkActivity(mTestPage));
        assertHistogramsRecorded(1, WEBAPK_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mWebApkActivityTestRule, mTestPage2);
        assertHistogramsRecorded(1, WEBAPK_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are recorded in case of intent coming from an
     * external app.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "https://crbug.com/1023433")
    public void testFromExternalAppRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityFromExternalApp(mTestPage, null));
        assertHistogramsRecorded(1, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, mTestPage2);
        assertHistogramsRecorded(1, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the NTP.
     */
    @Test
    @LargeTest
    public void testNTPNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityFromLauncher());
        assertHistogramsRecorded(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, mTestPage2);
        assertHistogramsRecorded(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the blank
     * page.
     */
    @Test
    @LargeTest
    public void testBlankPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityOnBlankPage());
        assertHistogramsRecorded(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, mTestPage2);
        assertHistogramsRecorded(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1062055")
    public void testErrorPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityWithURL(mErrorPage));
        assertHistogramsRecorded(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, mTestPage2);
        assertHistogramsRecorded(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    public void testWebApkErrorPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mWebApkActivityTestRule.startWebApkActivity(mErrorPage));
        assertHistogramsRecorded(0, WEBAPK_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mWebApkActivityTestRule, mTestPage2);
        assertHistogramsRecorded(0, WEBAPK_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded if the application is in
     * background at the time of the page loading.
     */
    @Test
    @LargeTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1062055")
    public void testBackgroundedPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(() -> {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.addCategory(Intent.CATEGORY_LAUNCHER);

            // mSlowPage will hang for 2 seconds before sending a response. It should be enough to
            // put Chrome in background before the page is committed.
            mTabbedActivityTestRule.prepareUrlIntent(intent, mSlowPage);
            mTabbedActivityTestRule.launchActivity(intent);

            // Put Chrome in background before the page is committed.
            ChromeApplicationTestUtils.fireHomeScreenIntent(
                    InstrumentationRegistry.getTargetContext());

            // Wait for a tab to be loaded.
            mTabbedActivityTestRule.waitForActivityNativeInitializationComplete();
            CriteriaHelper.pollUiThread(
                    ()
                            -> mTabbedActivityTestRule.getActivity().getActivityTab() != null,
                    "Tab never selected/initialized.");
            Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
            ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
        });
        assertHistogramsRecorded(0, TABBED_SUFFIX);
        runAndWaitForPageLoadMetricsRecorded(() -> {
            // Put Chrome in foreground before loading a new page.
            ChromeApplicationTestUtils.launchChrome(InstrumentationRegistry.getTargetContext());
            mTabbedActivityTestRule.loadUrl(mTestPage);
        });
        assertHistogramsRecorded(0, TABBED_SUFFIX);
    }
}
