// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.WebFeature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.ContentWebFeatureUsageUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Integration tests for {@link ContentWebFeatureUsageUtils}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContentWebFeatureUsageUtilsIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        mActivityTestRule.startMainActivityWithURL(
                testServer.getURL("/chrome/test/data/android/simple.html"));
    }

    @Test
    @SmallTest
    public void testLogWebFeatureForCurrentPage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.UseCounter.Features", WebFeature.IDENTITY_DIGITAL_CREDENTIALS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContentWebFeatureUsageUtils.logWebFeatureForCurrentPage(
                            mActivityTestRule.getWebContents(),
                            WebFeature.IDENTITY_DIGITAL_CREDENTIALS);
                });
        histogramWatcher.assertExpected();
    }
}
