// Copyright 2017 The Chromium Authors
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.jank_tracker.JankMetricUMARecorder;
import org.chromium.base.jank_tracker.JankMetricUMARecorderJni;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
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
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for startup timing histograms.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "These startup tests rely on having exactly one process start per test.")
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class StartupLoadingMetricsTest {
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
    private static final String FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM =
            "Startup.Android.Cold.FirstNavigationCommitOccurredPreForeground";

    private static final String TABBED_SUFFIX = ChromeTabbedActivity.STARTUP_UMA_HISTOGRAM_SUFFIX;
    private static final String WEBAPK_SUFFIX =
            WebApkActivityLifecycleUmaTracker.STARTUP_UMA_HISTOGRAM_SUFFIX;

    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public WebApkActivityTestRule mWebApkActivityTestRule = new WebApkActivityTestRule();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    JankMetricUMARecorder.Natives mJankRecorderNativeMock;

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
        mJniMocker.mock(JankMetricUMARecorderJni.TEST_HOOKS, mJankRecorderNativeMock);
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
                () -> PageLoadMetrics.addObserver(testObserver, false));
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

        if (expectedCount > 0) {
            // If the first nav commit was recorded, it should have also been registered as having
            // occurred post-foregrounding (since otherwise it would not have been recorded).
            Assert.assertEquals(expectedCount,
                    RecordHistogram.getHistogramValueCountForTesting(
                            FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, 0));
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, 1));
        } else {
            // Note that the first commit might or might not have occurred in this case depending on
            // the test. However, if it occurred it must have occurred pre-foregrounding (since
            // otherwise the core metric would have been recorded).
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, 0));
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
                () -> mTabbedActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL));
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

    @Test
    @LargeTest
    public void testRecordingOfFirstNavigationCommitPreForeground() throws Exception {
        UmaUtils.skipRecordingNextForegroundStartTimeForTesting();

        runAndWaitForPageLoadMetricsRecorded(() -> {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.addCategory(Intent.CATEGORY_LAUNCHER);
            // Waits for the native initialization to finish. As part of it skips the foreground
            // start as requested above.
            mTabbedActivityTestRule.startMainActivityFromIntent(intent, mTestPage);
        });

        // Startup metrics should not have been recorded since the browser does not know it is in
        // the foreground.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + TABBED_SUFFIX));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));

        // The metric for the first navigation commit having occurred pre-foregrounding should also
        // not have been recorded at this point, as there hasn't yet been a notification that the
        // browser has come to the foreground.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, 1));

        // Trigger the come-to-foreground event. This time it should not be skipped.
        TestThreadUtils.runOnUiThreadBlocking(UmaUtils::recordForegroundStartTimeWithNative);

        // Startup metrics should still not have been recorded...
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + TABBED_SUFFIX));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));

        // ...but the metric for the first navigation commit having occurred pre-foregrounding
        // *should* now have been recorded.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, 1));
    }
}
