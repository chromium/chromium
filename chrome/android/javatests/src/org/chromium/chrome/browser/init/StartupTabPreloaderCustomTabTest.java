// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

/**
 * Browser tests for {@link StartupTabPreloader}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class StartupTabPreloaderCustomTabTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TAB_LOADED_HISTOGRAM =
            "Startup.Android.StartupTabPreloader.TabLoaded";
    private static final String TAB_TAKEN_HISTOGRAM =
            "Startup.Android.StartupTabPreloader.TabTaken";

    @Rule
    public CustomTabActivityTestRule mActivityRule = new CustomTabActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mServerRule = new EmbeddedTestServerRule();

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS)
    public void testStartupTabPreloaderWithCustomTab() throws Exception {
        Uri uri = Uri.parse(mServerRule.getServer().getURL(TEST_PAGE));
        Intent customTabActivityIntent = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            return LaunchIntentDispatcher.createCustomTabActivityIntent(
                    InstrumentationRegistry.getTargetContext(), intent);
        });

        mActivityRule.startCustomTabActivityWithIntent(customTabActivityIntent);

        // The StartupTabPreloader should have loaded a url.
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_LOADED_HISTOGRAM, 1));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(TAB_TAKEN_HISTOGRAM, 1));
    }
}
