// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
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

    @Rule
    public ChromeTabbedActivityTestRule mActivityRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mServerRule = new EmbeddedTestServerRule();

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
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
    @DisableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
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
    @EnableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
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
    @EnableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
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
    @EnableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
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
    @EnableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
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

        intent = new Intent(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        mActivityRule.startMainActivityFromIntent(
                intent, mServerRule.getServer().getURL(TEST_PAGE2));

        // The second intent should be ignored and not increment the counters.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
    }
}
