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
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
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
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationStart.Load";
    private static final String PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_NO_PRELOAD =
            "Android.StartupTabPreloader.LoadDecisionToFirstNavigationStart.NoLoad";

    @Rule
    public ChromeTabbedActivityTestRule mActivityRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mServerRule = new EmbeddedTestServerRule();

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
    }

    @Test
    @LargeTest
    @FlakyTest(message = "https://crbug.com/1271158")
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabTaken()
            throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

        // The StartupTabPreloader should have loaded a url.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));

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
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_PRELOAD));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_NO_PRELOAD));
    }

    @Test
    @LargeTest
    @FlakyTest(message = "https://crbug.com/1271158")
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabDropped()
            throws Exception {
        StartupTabPreloader.failNextTabMatchForTesting();

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE));

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
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_PRELOAD));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_NO_PRELOAD));
    }

    @Test
    @LargeTest
    @FlakyTest(message = "https://crbug.com/1271158")
    @DisableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    public void testStartupTabPreloaderStartupLoadingMetricsRecordedWhenTabNotPreloaded()
            throws Exception {
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
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_PRELOAD));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        PRELOAD_TRIGGER_TO_FIRST_NAVIGATION_START_NO_PRELOAD));
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)
    @DisabledTest(message = "https://crbug.com/1012479")
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
    }
}
