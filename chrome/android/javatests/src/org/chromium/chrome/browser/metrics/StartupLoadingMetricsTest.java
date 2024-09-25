// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;
import android.content.Intent;

import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.LauncherShortcutActivity;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetricsTest;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebApkActivityTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Tests for startup timing histograms. */
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
    private static final String FIRST_COMMIT_HISTOGRAM2 =
            "Startup.Android.Cold.TimeToFirstNavigationCommit2.Tabbed";
    private static final String FIRST_CONTENTFUL_PAINT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstContentfulPaint";
    private static final String FIRST_CONTENTFUL_PAINT_HISTOGRAM3 =
            "Startup.Android.Cold.TimeToFirstContentfulPaint3";
    private static final String FIRST_VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstVisibleContent";
    private static final String FIRST_VISIBLE_CONTENT_HISTOGRAM2 =
            "Startup.Android.Cold.TimeToFirstVisibleContent2";
    private static final String FIRST_VISIBLE_CONTENT_COLD_HISTOGRAM4 =
            "Startup.Android.Cold.TimeToFirstVisibleContent4";
    private static final String VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToVisibleContent";
    private static final String FIRST_COMMIT_COLD_HISTOGRAM3 =
            "Startup.Android.Cold.TimeToFirstNavigationCommit3";
    private static final String MAIN_INTENT_COLD_START_HISTOGRAM =
            "Startup.Android.MainIntentIsColdStart";

    private CustomTabsConnection mConnectionToCleanup;

    private static final String TABBED_SUFFIX = ".Tabbed";
    private static final String WEB_APK_SUFFIX = ".WebApk";

    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public WebApkActivityTestRule mWebApkActivityTestRule = new WebApkActivityTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() {
        ColdStartTracker.setStartedAsColdForTesting();
        SimpleStartupForegroundSessionDetector.resetForTesting();
    }

    @After
    public void tearDown() {
        if (mConnectionToCleanup != null) {
            CustomTabsTestUtils.cleanupSessions(mConnectionToCleanup);
        }
    }

    private String getServerURL(String url) {
        return mTabbedActivityTestRule.getTestServer().getURL(url);
    }

    private String getTestPage() {
        return getServerURL(TEST_PAGE);
    }

    private String getTestPage2() {
        return getServerURL(TEST_PAGE_2);
    }

    private void runAndWaitForPageLoadMetricsRecorded(Runnable runnable) throws Exception {
        PageLoadMetricsTest.PageLoadMetricsTestObserver testObserver =
                new PageLoadMetricsTest.PageLoadMetricsTestObserver();
        ThreadUtils.runOnUiThreadBlocking(() -> PageLoadMetrics.addObserver(testObserver, false));
        runnable.run();
        // First Contentful Paint may be recorded asynchronously after a page load is finished, we
        // have to wait the event to occur.
        testObserver.waitForFirstContentfulPaintEvent();
        ThreadUtils.runOnUiThreadBlocking(() -> PageLoadMetrics.removeObserver(testObserver));
    }

    private void loadUrlAndWaitForPageLoadMetricsRecorded(
            ChromeActivityTestRule chromeActivityTestRule, String url) throws Exception {
        runAndWaitForPageLoadMetricsRecorded(() -> chromeActivityTestRule.loadUrl(url));
    }

    private void assertHistogramsRecordedWithForegroundStart(
            int expectedCount, String histogramSuffix) {
        assertHistogramsRecordedAsExpected(expectedCount, histogramSuffix);
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.Cold.TimeToForegroundSessionStart"));
    }

    private void assertMainIntentLaunchColdStartHistogramRecorded(int expectedCount) {
        Assert.assertEquals(
                expectedCount,
                RecordHistogram.getHistogramTotalCountForTesting(MAIN_INTENT_COLD_START_HISTOGRAM));
    }

    private void assertHistogramsRecordedAsExpected(int expectedCount, String histogramSuffix) {
        boolean isTabbedSuffix = histogramSuffix.equals(TABBED_SUFFIX);

        // Check that the new first navigation commit events are recorded for the tabbed activity.
        Assert.assertEquals(
                isTabbedSuffix ? expectedCount : 0,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM2));

        int coldStartFirstCommit4Samples =
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_COLD_HISTOGRAM3 + histogramSuffix);
        Assert.assertTrue(coldStartFirstCommit4Samples < 2);

        int coldStartFirstContentfulPaintSamples =
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_CONTENTFUL_PAINT_HISTOGRAM3);
        Assert.assertTrue(coldStartFirstContentfulPaintSamples < 2);

        int firstCommitSamples =
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + histogramSuffix);
        Assert.assertTrue(firstCommitSamples < 2);

        int firstContentfulPaintSamples =
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_CONTENTFUL_PAINT_HISTOGRAM + histogramSuffix);
        Assert.assertTrue(firstContentfulPaintSamples < 2);

        int visibleContentSamples =
                RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM);
        Assert.assertTrue(visibleContentSamples < 2);

        if (expectedCount == 1 && firstCommitSamples == 0) {
            // The startup FCP and 'visible content' also record their samples depending on how fast
            // they happen in relation to the post-native initialization.
            Assert.assertTrue(firstCommitSamples <= firstContentfulPaintSamples);
            Assert.assertTrue(firstCommitSamples <= visibleContentSamples);
        } else {
            // Once the racy commit case is excluded, the histograms should record the expected
            // number of samples.
            Assert.assertEquals(expectedCount, firstCommitSamples);
            Assert.assertEquals(expectedCount, firstContentfulPaintSamples);
            if (isTabbedSuffix) {
                Assert.assertEquals(expectedCount, visibleContentSamples);
            }
        }

        if (isTabbedSuffix) {
            // These tests only exercise the cases when the first visible content is calculated as
            // the first navigation commit.
            Assert.assertEquals(
                    firstCommitSamples,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            FIRST_VISIBLE_CONTENT_HISTOGRAM));
            Assert.assertEquals(
                    expectedCount,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            FIRST_VISIBLE_CONTENT_HISTOGRAM2));
            Assert.assertEquals(
                    coldStartFirstCommit4Samples,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            FIRST_VISIBLE_CONTENT_COLD_HISTOGRAM4));
        }
    }

    /** Tests cold start metrics for main icon launches recorded correctly. */
    @Test
    @LargeTest
    public void testStartWithMainLauncerIconRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityFromLauncher());
        assertMainIntentLaunchColdStartHistogramRecorded(1);
    }

    /** Tests cold start metrics for main icon shortcut launches recorded correctly. */
    @Test
    @LargeTest
    public void testStartWithMainLauncerShortcutRecorded() throws Exception {
        Intent intent = new Intent(LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB);
        intent.setClass(ContextUtils.getApplicationContext(), LauncherShortcutActivity.class);
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityFromIntent(intent, null));
        assertMainIntentLaunchColdStartHistogramRecorded(1);
    }

    /**
     * Tests that the startup loading histograms are recorded only once on startup. Tabbed Activity
     * version.
     */
    @Test
    @LargeTest
    public void testStartWithURLRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityWithURL(getTestPage()));
        assertHistogramsRecordedWithForegroundStart(1, TABBED_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, getTestPage2());
        assertHistogramsRecordedWithForegroundStart(1, TABBED_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
    }

    /**
     * Tests that the startup loading histograms are recorded only once on startup. WebAPK version.
     */
    @Test
    @LargeTest
    public void testWebApkStartRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mWebApkActivityTestRule.startWebApkActivity(getTestPage()));
        assertHistogramsRecordedWithForegroundStart(1, WEB_APK_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mWebApkActivityTestRule, getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(1, WEB_APK_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are recorded in case of intent coming from an
     * external app. Also checks that they are recorded only once.
     */
    @Test
    @LargeTest
    public void testFromExternalAppRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () ->
                        mTabbedActivityTestRule.startMainActivityFromExternalApp(
                                getTestPage(), null));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(1, TABBED_SUFFIX);

        // Check that no new histograms were recorded on the second navigation.
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(1, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the NTP.
     */
    @Test
    @LargeTest
    public void testNtpNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
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
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    public void testErrorPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityWithURL(getServerURL(ERROR_PAGE)));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mTabbedActivityTestRule, getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    public void testWebApkErrorPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mWebApkActivityTestRule.startWebApkActivity(getServerURL(ERROR_PAGE)));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, WEB_APK_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mWebApkActivityTestRule, getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedWithForegroundStart(0, WEB_APK_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded if the application is in
     * background at the time of the page loading.
     */
    @Test
    @LargeTest
    public void testBackgroundedPageNotRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addCategory(Intent.CATEGORY_LAUNCHER);

                    // The SLOW_PAGE will hang for 2 seconds before sending a response. It should be
                    // enough to put Chrome in background before the page is committed.
                    mTabbedActivityTestRule.prepareUrlIntent(intent, getServerURL(SLOW_PAGE));
                    mTabbedActivityTestRule.launchActivity(intent);

                    // Put Chrome in background before the page is committed.
                    ChromeApplicationTestUtils.fireHomeScreenIntent(
                            ApplicationProvider.getApplicationContext());

                    // Wait for a tab to be loaded.
                    mTabbedActivityTestRule.waitForActivityNativeInitializationComplete();
                    CriteriaHelper.pollUiThread(
                            () -> mTabbedActivityTestRule.getActivity().getActivityTab() != null,
                            "Tab never selected/initialized.");
                    Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
                    ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
                });
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);

        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    // Put Chrome in foreground before loading a new page.
                    ChromeApplicationTestUtils.launchChrome(
                            ApplicationProvider.getApplicationContext());
                    mTabbedActivityTestRule.loadUrl(getTestPage());
                });
        assertMainIntentLaunchColdStartHistogramRecorded(1);
        assertHistogramsRecordedWithForegroundStart(0, TABBED_SUFFIX);
    }

    @Test
    @LargeTest
    public void testRecordingOfFirstNavigationCommitPreForeground() throws Exception {
        UmaUtils.skipRecordingNextForegroundStartTimeForTesting();

        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addCategory(Intent.CATEGORY_LAUNCHER);
                    // Waits for the native initialization to finish. As part of it skips the
                    // foreground start as requested above.
                    mTabbedActivityTestRule.startMainActivityFromIntent(intent, getTestPage());
                });

        // Startup metrics should not have been recorded since the browser does not know it is in
        // the foreground.
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + TABBED_SUFFIX));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
        assertMainIntentLaunchColdStartHistogramRecorded(0);

        // The metric based on early foreground notification should be recorded.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM2));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_COLD_HISTOGRAM3 + TABBED_SUFFIX));
        // The startup time is not zero.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(FIRST_COMMIT_HISTOGRAM2, 0));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        FIRST_COMMIT_COLD_HISTOGRAM3 + TABBED_SUFFIX, 0));

        // Trigger the come-to-foreground event. This time it should not be skipped.
        ThreadUtils.runOnUiThreadBlocking(UmaUtils::recordForegroundStartTimeWithNative);

        // Startup metrics should still not have been recorded...
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + TABBED_SUFFIX));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
    }

    @Test
    @LargeTest
    public void testCustomTabs() throws Exception {
        // Prepare CCT connection and intent.
        Context context =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        CustomTabsConnection connection = CustomTabsTestUtils.setUpConnection();
        mConnectionToCleanup = connection;
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(token, false);
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, getTestPage());

        // Load URL in CCT.
        runAndWaitForPageLoadMetricsRecorded(
                () -> mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

        // Verify the URL and check that startup metrics are *not* recorded.
        Assert.assertEquals(getTestPage(), ChromeTabUtils.getUrlStringOnUiThread(tab));
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
        assertHistogramsRecordedAsExpected(0, WEB_APK_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);

        // Pretend that it is a cold start to ensure in the following checks that the foreground
        // session is discarded when the CCT hides.
        SimpleStartupForegroundSessionDetector.resetForTesting();
        ColdStartTracker.setStartedAsColdForTesting();

        // Load another URL in a tabbed activity and check that startup metrics are still not
        // recorded.
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startMainActivityWithURL(TEST_PAGE_2));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
        assertHistogramsRecordedAsExpected(0, WEB_APK_SUFFIX);
    }
}
