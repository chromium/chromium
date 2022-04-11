// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

/**
 * Browser tests for {@link StartupTabPreloader}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class StartupTabPreloaderTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE2 = "/chrome/test/data/android/about.html";
    private static final String TAB_LOADED_HISTOGRAM =
            "Startup.Android.StartupTabPreloader.TabLoaded";
    private static final String TAB_TAKEN_HISTOGRAM =
            "Startup.Android.StartupTabPreloader.TabTaken";
    private static final String FIRST_COMMIT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstNavigationCommit"
            + ChromeTabbedActivity.STARTUP_UMA_HISTOGRAM_SUFFIX;
    private static final String FIRST_CONTENTFUL_PAINT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstContentfulPaint"
            + ChromeTabbedActivity.STARTUP_UMA_HISTOGRAM_SUFFIX;
    private static final String FIRST_VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToFirstVisibleContent";
    private static final String VISIBLE_CONTENT_HISTOGRAM =
            "Startup.Android.Cold.TimeToVisibleContent";
    private static final String ACTIVITY_START_TO_PRELOAD_TRIGGER =
            "Android.StartupTabPreloader.ActivityStartToLoadDecision";
    private static final String PRELOAD_TRIGGER_TO_MATCH_DECISION_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToMatchDecision.Load";
    private static final String PRELOAD_TRIGGER_TO_MATCH_DECISION_NO_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToMatchDecision.NoLoad";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationStart.Load";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_NO_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationStart.NoLoad";
    private static final String PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_PRELOAD_BEFORE_MATCH =
            "Android.StartupTabPreloader.LoadDecisionToFirstVisibleContent.LoadPreMatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_PRELOAD_AND_TAKE =
            "Android.StartupTabPreloader.LoadDecisionToFirstVisibleContent.LoadAndMatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_PRELOAD_AND_DROP =
            "Android.StartupTabPreloader.LoadDecisionToFirstVisibleContent.LoadAndMismatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_NO_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstVisibleContent.NoLoad";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_PRELOAD_BEFORE_MATCH =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationCommit.LoadPreMatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_PRELOAD_AND_TAKE =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationCommit.LoadAndMatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_PRELOAD_AND_DROP =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationCommit.LoadAndMismatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_NO_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationCommit.NoLoad";
    private static final String PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_PRELOAD_BEFORE_MATCH =
            "Android.StartupTabPreloader.LoadDecisionToFirstContentfulPaint.LoadPreMatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_PRELOAD_AND_TAKE =
            "Android.StartupTabPreloader.LoadDecisionToFirstContentfulPaint.LoadAndMatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_PRELOAD_AND_DROP =
            "Android.StartupTabPreloader.LoadDecisionToFirstContentfulPaint.LoadAndMismatch";
    private static final String PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_NO_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstContentfulPaint.NoLoad";
    private static final String LOAD_DECISION_REASON =
            "Startup.Android.StartupTabPreloader.LoadDecisionReason";

    // Used for verifying expected histogram counts.
    private static final int NO_PRELOAD = 0;
    private static final int PRELOAD = 1;
    private static final int NO_PRELOAD_MATCH = 0;
    private static final int PRELOAD_MATCH = 1;

    @Rule
    public ChromeTabbedActivityTestRule mActivityRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mServerRule = new EmbeddedTestServerRule();

    // Verifies the state of various startup metrics being recorded appropriately for the varues of
    // |preload| and |preloadMatch| (specified by the constants above).
    private void assertStartupMetricsRecorded(int preload, int preloadMatch) {
        int noPreload = 1 - preload;
        int preloadMismatch = (preload == 1 && preloadMatch == 0) ? 1 : 0;

        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACTIVITY_START_TO_PRELOAD_TRIGGER));

        Assert.assertEquals(preload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_PRELOAD));
        Assert.assertEquals(noPreload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_NO_PRELOAD));

        Assert.assertEquals(preload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_MATCH_DECISION_PRELOAD));
        Assert.assertEquals(noPreload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_MATCH_DECISION_NO_PRELOAD));

        Assert.assertEquals(preloadMatch,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_PRELOAD_AND_TAKE));
        Assert.assertEquals(preloadMismatch,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_PRELOAD_AND_DROP));
        Assert.assertEquals(noPreload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_NO_PRELOAD));

        Assert.assertEquals(preloadMatch,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_PRELOAD_AND_TAKE));
        Assert.assertEquals(preloadMismatch,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_PRELOAD_AND_DROP));
        Assert.assertEquals(noPreload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_NO_PRELOAD));

        Assert.assertEquals(preloadMatch,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_PRELOAD_AND_TAKE));
        Assert.assertEquals(preloadMismatch,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_PRELOAD_AND_DROP));
        Assert.assertEquals(noPreload,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_NO_PRELOAD));

        // This case never triggers in these tests.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_VISIBLE_CONTENT_PRELOAD_BEFORE_MATCH));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_COMMIT_PRELOAD_BEFORE_MATCH));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_CONTENTFUL_PAINT_PRELOAD_BEFORE_MATCH));
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderWithViewIntent() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should have loaded a url.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.ALL_SATISFIED));
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    @DisabledTest(message = "https://crbug.com/1271158")
    public void testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabTaken()
            throws Exception {
        // Force the browser to regard itself as being in the foreground to work around the
        // fact that the navigation here can happen before ChromeActivity records the
        // browser as being in the foreground, in which case startup metrics are erroneously
        // not recorded. TODO(crbug.com/1273097): Eliminate this call when we fix startup
        // metrics to be recorded in this case.
        TestThreadUtils.runOnUiThreadBlocking(() -> UmaUtils.recordForegroundStartTime());

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should have loaded a url.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.ALL_SATISFIED));

        // First contentful paint should be recorded.
        CriteriaHelper.pollUiThread(()
                                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                                       FIRST_CONTENTFUL_PAINT_HISTOGRAM)
                        == 1);
        // First contentful paint is the last startup metric to be recorded, so the other startup
        // metrics should also have been recorded at this point.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM));

        // Startup tab preload-specific startup metrics should also have been recorded.
        assertStartupMetricsRecorded(PRELOAD, PRELOAD_MATCH);
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabDropped()
            throws Exception {
        // Force the browser to regard itself as being in the foreground to work around the
        // fact that the navigation here can happen before ChromeActivity records the
        // browser as being in the foreground, in which case startup metrics are erroneously
        // not recorded. TODO(crbug.com/1273097): Eliminate this call when we fix startup
        // metrics to be recorded in this case.
        TestThreadUtils.runOnUiThreadBlocking(() -> UmaUtils.recordForegroundStartTime());

        StartupTabPreloader.failNextTabMatchForTesting();

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.ALL_SATISFIED));

        // The StartupTabPreloader should have loaded a url, but it should not have been taken.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));

        // First contentful paint should be recorded.
        CriteriaHelper.pollUiThread(()
                                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                                       FIRST_CONTENTFUL_PAINT_HISTOGRAM)
                        == 1);

        // First contentful paint is the last startup metric to be recorded, so the other startup
        // metrics should also have been recorded at this point.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM));

        // Startup tab preload-specific startup metrics should also have been recorded.
        assertStartupMetricsRecorded(PRELOAD, NO_PRELOAD_MATCH);
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabNotPreloaded()
            throws Exception {
        // Force the browser to regard itself as being in the foreground to work around the
        // fact that the navigation here can happen before ChromeActivity records the
        // browser as being in the foreground, in which case startup metrics are erroneously
        // not recorded. TODO(crbug.com/1273097): Eliminate this call when we fix startup
        // metrics to be recorded in this case.
        TestThreadUtils.runOnUiThreadBlocking(() -> UmaUtils.recordForegroundStartTime());

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.putExtra(StartupTabPreloader.EXTRA_DISABLE_STARTUP_TAB_PRELOADER, true);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should not have loaded a url.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.DISABLED_BY_INTENT));

        // First contentful paint should be recorded.
        CriteriaHelper.pollUiThread(()
                                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                                       FIRST_CONTENTFUL_PAINT_HISTOGRAM)
                        == 1);
        // First contentful paint is the last startup metric to be recorded, so the other startup
        // metrics should also have been recorded at this point.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM));

        // Startup tab preload-specific startup metrics should also have been recorded.
        assertStartupMetricsRecorded(NO_PRELOAD, NO_PRELOAD_MATCH);
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void
    testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabWouldBeTakenIfNotPreventedByFeature()
            throws Exception {
        // Force the browser to regard itself as being in the foreground to work around the
        // fact that the navigation here can happen before ChromeActivity records the
        // browser as being in the foreground, in which case startup metrics are erroneously
        // not recorded. TODO(crbug.com/1273097): Eliminate this call when we fix startup
        // metrics to be recorded in this case.
        TestThreadUtils.runOnUiThreadBlocking(() -> UmaUtils.recordForegroundStartTime());

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should not have actually loaded a url.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.DISABLED_BY_FEATURE));

        // First contentful paint should be recorded.
        CriteriaHelper.pollUiThread(()
                                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                                       FIRST_CONTENTFUL_PAINT_HISTOGRAM)
                        == 1);
        // First contentful paint is the last startup metric to be recorded, so the other startup
        // metrics should also have been recorded at this point.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM));

        // Startup tab preload-specific startup metrics should also have been recorded, with the
        // state reflecting what it would have been if startup tab preloading were not prevented by
        // the base::Feature.
        assertStartupMetricsRecorded(PRELOAD, PRELOAD_MATCH);
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void
    testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabWouldBeDroppedIfNotPreventedByFeature()
            throws Exception {
        // Force the browser to regard itself as being in the foreground to work around the
        // fact that the navigation here can happen before ChromeActivity records the
        // browser as being in the foreground, in which case startup metrics are erroneously
        // not recorded. TODO(crbug.com/1273097): Eliminate this call when we fix startup
        // metrics to be recorded in this case.
        TestThreadUtils.runOnUiThreadBlocking(() -> UmaUtils.recordForegroundStartTime());

        StartupTabPreloader.failNextTabMatchForTesting();

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should not have actually loaded a url.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.DISABLED_BY_FEATURE));

        // First contentful paint should be recorded.
        CriteriaHelper.pollUiThread(()
                                            -> RecordHistogram.getHistogramTotalCountForTesting(
                                                       FIRST_CONTENTFUL_PAINT_HISTOGRAM)
                        == 1);

        // First contentful paint is the last startup metric to be recorded, so the other startup
        // metrics should also have been recorded at this point.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(FIRST_COMMIT_HISTOGRAM));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(FIRST_VISIBLE_CONTENT_HISTOGRAM));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting(VISIBLE_CONTENT_HISTOGRAM));

        // Startup tab preload-specific startup metrics should also have been recorded, with the
        // state reflecting what it would have been if startup tab preloading were not prevented by
        // the base::Feature.
        assertStartupMetricsRecorded(PRELOAD, NO_PRELOAD_MATCH);
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderWithViewIntentFeatureDisabled() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should have ignored the intent.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderWithIncognitoViewIntent() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // Incognito requests should be ignored.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOAD_DECISION_REASON, StartupTabPreloader.LoadDecisionReason.INCOGNITO));
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderWithMainIntentWithUrl() throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should have loaded a url.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.ALL_SATISFIED));
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderWithMainIntentWithoutUrl() throws Exception {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        mActivityRule.startMainActivityFromIntent(intent, null);

        // There is no url so the StartupTabPreloader should ignore it.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LOAD_DECISION_REASON, StartupTabPreloader.LoadDecisionReason.NO_URL));
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderWithMultipleViewIntents() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should have loaded a url.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.ALL_SATISFIED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACTIVITY_START_TO_PRELOAD_TRIGGER));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_MATCH_DECISION_PRELOAD));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_MATCH_DECISION_NO_PRELOAD));

        Tab currentTab = mActivityRule.getActivity().getActivityTab();

        intent = new Intent(Intent.ACTION_VIEW);
        String url = mServerRule.getServer().getURL(TEST_PAGE2);
        mActivityRule.getActivity().startActivity(mActivityRule.prepareUrlIntent(intent, url));

        CriteriaHelper.pollUiThread(
                () -> mActivityRule.getActivity().getActivityTab() != currentTab);
        ChromeTabUtils.waitForTabPageLoaded(mActivityRule.getActivity().getActivityTab(), url);

        // The second intent should be ignored and not increment the counters.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(LOAD_DECISION_REASON,
                        StartupTabPreloader.LoadDecisionReason.ALL_SATISFIED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        ACTIVITY_START_TO_PRELOAD_TRIGGER));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_MATCH_DECISION_PRELOAD));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_MATCH_DECISION_NO_PRELOAD));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderMultipleTabCreationWithFeatureEnabled() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should not have loaded a url.
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));

        Tab currentTab = mActivityRule.getActivity().getActivityTab();

        // Creating a second tab should not cause a browser crash. Verifies safety of subtle
        // logic handling metrics in the case where the feature is enabled.
        intent = new Intent(Intent.ACTION_VIEW);
        String url = mServerRule.getServer().getURL(TEST_PAGE2);
        mActivityRule.getActivity().startActivity(mActivityRule.prepareUrlIntent(intent, url));

        CriteriaHelper.pollUiThread(
                () -> mActivityRule.getActivity().getActivityTab() != currentTab);
        ChromeTabUtils.waitForTabPageLoaded(mActivityRule.getActivity().getActivityTab(), url);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                0, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
    }
}
