// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;
import android.content.Intent;
import android.os.Build;

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
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.LauncherShortcutActivity;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
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
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;

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
    private static final String FIRST_VISIBLE_CONTENT_HISTOGRAM2 =
            "Startup.Android.Cold.TimeToFirstVisibleContent2";
    private static final String FIRST_VISIBLE_CONTENT_COLD_HISTOGRAM4 =
            "Startup.Android.Cold.TimeToFirstVisibleContent4";
    private static final String VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToVisibleContent";
    private static final String TIME_TO_STARTUP_FCP_OR_PAINT_PREVIEW_HISTOGRAM =
            "Startup.Android.Cold.TimeToStartupFcpOrPaintPreview";
    private static final String FIRST_COMMIT_COLD_HISTOGRAM3 =
            "Startup.Android.Cold.TimeToFirstNavigationCommit3";
    private static final String MAIN_INTENT_COLD_START_HISTOGRAM =
            "Startup.Android.MainIntentIsColdStart";
    private static final String MAIN_INTENT_TIME_TO_FIRST_DRAW_WARM_MS_HISTOGRAM =
            "Startup.Android.Warm.MainIntentTimeToFirstDraw";
    private static final String NTP_TIME_TO_FIRST_DRAW_COLD_HISTOGRAM =
            "Startup.Android.Cold.NewTabPage.TimeToFirstDraw";
    private static final String NTP_COLD_START_BINDER_HISTOGRAM =
            "Startup.Android.Cold.NewTabPage.TimeSpentInBinder";
    private static final String COLD_START_TIME_TO_FIRST_FRAME =
            "Startup.Android.Cold.TimeToFirstFrame";

    private static final String TABBED_SUFFIX = ".Tabbed";
    private static final String WEB_APK_SUFFIX = ".WebApk";

    private static final boolean APPLICATION_START_INFO_SUPPORTED =
            (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM);

    private CustomTabsConnection mConnectionToCleanup;

    @Rule
    public FreshCtaTransitTestRule mTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public WebApkActivityTestRule mWebApkActivityTestRule = new WebApkActivityTestRule();

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
            CustomTabsTestUtils.cleanupSessions();
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

    private void waitForHistogram(HistogramWatcher histogramWatcher) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        histogramWatcher.assertExpected();
                        return true;
                    } catch (AssertionError e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                "waitForHistogram timeout",
                10000,
                200);
    }

    private static HistogramWatcher createNtpColdStartHistogramWatcher(int expectedCount) {
        return HistogramWatcher.newBuilder()
                .expectAnyRecordTimes(NTP_TIME_TO_FIRST_DRAW_COLD_HISTOGRAM, expectedCount)
                .build();
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
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_CONTENTFUL_PAINT_HISTOGRAM3 + histogramSuffix);
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
        int timeToStartupFcpOrPaintPreviewSamples =
                RecordHistogram.getHistogramTotalCountForTesting(
                        TIME_TO_STARTUP_FCP_OR_PAINT_PREVIEW_HISTOGRAM);
        Assert.assertTrue(visibleContentSamples < 2);
        Assert.assertTrue(timeToStartupFcpOrPaintPreviewSamples < 2);

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
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(1);
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startFromLauncherAtNtp());
        assertMainIntentLaunchColdStartHistogramRecorded(1);
        waitForHistogram(ntpColdStartWatcher);
    }

    /** Tests cold start metrics for main icon shortcut launches recorded correctly. */
    @Test
    @LargeTest
    public void testStartWithMainLauncerShortcutRecorded() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(NTP_TIME_TO_FIRST_DRAW_COLD_HISTOGRAM, 0)
                        .expectAnyRecordTimes(
                                COLD_START_TIME_TO_FIRST_FRAME,
                                APPLICATION_START_INFO_SUPPORTED ? 1 : 0)
                        .build();
        Intent intent = new Intent(LauncherShortcutActivity.ACTION_OPEN_NEW_INCOGNITO_TAB);
        intent.setClass(ContextUtils.getApplicationContext(), LauncherShortcutActivity.class);
        runAndWaitForPageLoadMetricsRecorded(
                () ->
                        mTabbedActivityTestRule
                                .startWithIntentPlusUrlTo(intent, null)
                                .arriveAt(
                                        IncognitoNewTabPageStation.newBuilder()
                                                .withEntryPoint()
                                                .build()));
        assertMainIntentLaunchColdStartHistogramRecorded(1);
        waitForHistogram(histogramWatcher);
    }

    /** Tests warm start metric for main icon launches recorded correctly. */
    @Test
    @LargeTest
    public void testWarmStartMainIntentTimeToFirstDrawRecordedCorrectly() throws Exception {
        // No records made for main intent cold starts.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(MAIN_INTENT_TIME_TO_FIRST_DRAW_WARM_MS_HISTOGRAM)
                        .expectAnyRecordTimes(
                                COLD_START_TIME_TO_FIRST_FRAME,
                                APPLICATION_START_INFO_SUPPORTED ? 1 : 0)
                        .build();

        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    mTabbedActivityTestRule.startFromLauncherAtNtp();
                    ChromeApplicationTestUtils.fireHomeScreenIntent(
                            mTabbedActivityTestRule.getActivity());
                });
        histogramWatcher.assertExpected();

        // Expect two records for two main intent warm starts.
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(MAIN_INTENT_TIME_TO_FIRST_DRAW_WARM_MS_HISTOGRAM, 2)
                        .expectAnyRecordTimes(COLD_START_TIME_TO_FIRST_FRAME, 0)
                        .build();
        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    ChromeApplicationTestUtils.fireHomeScreenIntent(
                            mTabbedActivityTestRule.getActivity());
                    try {
                        mTabbedActivityTestRule.resumeMainActivityFromLauncher();
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    ChromeApplicationTestUtils.fireHomeScreenIntent(
                            mTabbedActivityTestRule.getActivity());
                    try {
                        mTabbedActivityTestRule.resumeMainActivityFromLauncher();
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });

        // Go to home screen one more time since the metric is recorded during onPause()
        runAndWaitForPageLoadMetricsRecorded(
                () ->
                        ChromeApplicationTestUtils.fireHomeScreenIntent(
                                mTabbedActivityTestRule.getActivity()));
        histogramWatcher.assertExpected();
    }

    /**
     * Tests that the startup loading histograms are recorded only once on startup. Tabbed Activity
     * version.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "Flaky. See crbug.com/380204044")
    public void testStartWithURLRecorded() throws Exception {
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startOnUrl(getTestPage()));
        assertHistogramsRecordedAsExpected(1, TABBED_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(
                mTabbedActivityTestRule.getActivityTestRule(), getTestPage2());
        assertHistogramsRecordedAsExpected(1, TABBED_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
    }

    /**
     * Tests that the startup loading histograms are recorded only once on startup. WebAPK version.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/442398236")
    public void testWebApkStartRecorded() throws Exception {
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(0);
        runAndWaitForPageLoadMetricsRecorded(
                () -> mWebApkActivityTestRule.startWebApkActivity(getTestPage()));
        assertHistogramsRecordedAsExpected(1, WEB_APK_SUFFIX);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        loadUrlAndWaitForPageLoadMetricsRecorded(mWebApkActivityTestRule, getTestPage2());
        waitForHistogram(ntpColdStartWatcher);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(1, WEB_APK_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are recorded in case of intent coming from an
     * external app. Also checks that they are recorded only once.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "Flaky. See crbug.com/380204044")
    public void testFromExternalAppRecorded() throws Exception {
        String url = getTestPage();
        runAndWaitForPageLoadMetricsRecorded(
                () ->
                        mTabbedActivityTestRule.startWithIntentPlusUrlAtWebPage(
                                new Intent(Intent.ACTION_VIEW), url));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(1, TABBED_SUFFIX);

        // Check that no new histograms were recorded on the second navigation.
        loadUrlAndWaitForPageLoadMetricsRecorded(
                mTabbedActivityTestRule.getActivityTestRule(), getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(1, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are recorded correctly in case of navigation to the
     * NTP.
     */
    @Test
    @LargeTest
    public void testNtpRecordedCorrectly() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(NTP_TIME_TO_FIRST_DRAW_COLD_HISTOGRAM, 1)
                        .expectAnyRecordTimes(
                                COLD_START_TIME_TO_FIRST_FRAME,
                                APPLICATION_START_INFO_SUPPORTED ? 1 : 0)
                        .build();
        runAndWaitForPageLoadMetricsRecorded(() -> mTabbedActivityTestRule.startOnNtp());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(
                mTabbedActivityTestRule.getActivityTestRule(), getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        waitForHistogram(histogramWatcher);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
    }

    @Test
    @LargeTest
    public void testNtpBinderMetricRecordedCorrectly() throws Exception {
        HistogramWatcher ntpBinderWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(NTP_COLD_START_BINDER_HISTOGRAM, 1)
                        .build();
        runAndWaitForPageLoadMetricsRecorded(() -> mTabbedActivityTestRule.startOnNtp());
        waitForHistogram(ntpBinderWatcher);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the blank
     * page.
     */
    @Test
    @LargeTest
    public void testBlankPageNotRecorded() throws Exception {
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(0);
        runAndWaitForPageLoadMetricsRecorded(() -> mTabbedActivityTestRule.startOnBlankPage());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(
                mTabbedActivityTestRule.getActivityTestRule(), getTestPage2());
        waitForHistogram(ntpColdStartWatcher);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    public void testErrorPageNotRecorded() throws Exception {
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(0);
        runAndWaitForPageLoadMetricsRecorded(
                () -> mTabbedActivityTestRule.startOnTestServerUrl(ERROR_PAGE));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(
                mTabbedActivityTestRule.getActivityTestRule(), getTestPage2());
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        waitForHistogram(ntpColdStartWatcher);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded in case of navigation to the error
     * page.
     */
    @Test
    @LargeTest
    public void testWebApkErrorPageNotRecorded() throws Exception {
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(0);
        runAndWaitForPageLoadMetricsRecorded(
                () -> mWebApkActivityTestRule.startWebApkActivity(getServerURL(ERROR_PAGE)));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, WEB_APK_SUFFIX);
        loadUrlAndWaitForPageLoadMetricsRecorded(mWebApkActivityTestRule, getTestPage2());
        waitForHistogram(ntpColdStartWatcher);
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        assertHistogramsRecordedAsExpected(0, WEB_APK_SUFFIX);
    }

    /**
     * Tests that the startup loading histograms are not recorded if the application is in
     * background at the time of the page loading.
     */
    @Test
    @LargeTest
    public void testBackgroundedPageNotRecorded() throws Exception {
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(0);
        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addCategory(Intent.CATEGORY_LAUNCHER);

                    // The SLOW_PAGE will hang for 2 seconds before sending a response. It should be
                    // enough to put Chrome in background before the page is committed.
                    mTabbedActivityTestRule
                            .getActivityTestRule()
                            .prepareUrlIntent(intent, getServerURL(SLOW_PAGE));
                    mTabbedActivityTestRule.getActivityTestRule().launchActivity(intent);

                    // Put Chrome in background before the page is committed.
                    ChromeApplicationTestUtils.fireHomeScreenIntent(
                            ApplicationProvider.getApplicationContext());

                    // Wait for a tab to be loaded.
                    mTabbedActivityTestRule.waitForActivityNativeInitializationComplete();
                    CriteriaHelper.pollUiThread(
                            () -> mTabbedActivityTestRule.getActivityTab() != null,
                            "Tab never selected/initialized.");
                    Tab tab = mTabbedActivityTestRule.getActivityTab();
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
        waitForHistogram(ntpColdStartWatcher);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
    }

    @Test
    @LargeTest
    public void testRecordingOfFirstNavigationCommitPreForeground() throws Exception {
        UmaUtils.skipRecordingNextForegroundStartTimeForTesting();

        String url = getTestPage();
        runAndWaitForPageLoadMetricsRecorded(
                () -> {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.addCategory(Intent.CATEGORY_LAUNCHER);
                    // Waits for the native initialization to finish. As part of it skips the
                    // foreground start as requested above.
                    mTabbedActivityTestRule.startWithIntentPlusUrlAtWebPage(intent, url);
                });

        // Startup metrics should not have been recorded since the browser does not know it is in
        // the foreground.
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        FIRST_COMMIT_HISTOGRAM + TABBED_SUFFIX));
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
        var sessionHolder = new SessionHolder<>(token);
        connection.newSession(token);
        connection.setCanUseHiddenTabForSession(sessionHolder, false);
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, getTestPage());

        // Load URL in CCT.
        HistogramWatcher ntpColdStartWatcher = createNtpColdStartHistogramWatcher(0);
        runAndWaitForPageLoadMetricsRecorded(
                () -> mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent));
        Tab tab = mCustomTabActivityTestRule.getActivityTab();

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
        runAndWaitForPageLoadMetricsRecorded(() -> mTabbedActivityTestRule.startOnUrl(TEST_PAGE_2));
        assertMainIntentLaunchColdStartHistogramRecorded(0);
        waitForHistogram(ntpColdStartWatcher);
        assertHistogramsRecordedAsExpected(0, TABBED_SUFFIX);
        assertHistogramsRecordedAsExpected(0, WEB_APK_SUFFIX);
    }
}
