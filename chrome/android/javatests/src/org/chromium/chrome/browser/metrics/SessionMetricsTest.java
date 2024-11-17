// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.Context;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.url.JUnitTestGURLs;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Test interacts with activity startup & shutdown.")
public class SessionMetricsTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private static final String TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM =
            "Session.Android.TabbedSessionContainedGoogleSearch";

    private static final Long LOAD_URL_TOTAL_WAIT_TIME_SECONDS = 10L;

    private Context mAppContext;

    @Before
    public void setUp() {
        mAppContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
    }

    @Test
    @SmallTest
    public void testSessionContainedGoogleSearchPage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM, true)
                        .build();

        // Load Google SRP twice, but ensure histogram is only recorded to once.
        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        mTabbedActivityTestRule.waitForActivityNativeInitializationComplete();
        mTabbedActivityTestRule.loadUrl(
                JUnitTestGURLs.SEARCH_URL.getSpec(), LOAD_URL_TOTAL_WAIT_TIME_SECONDS);
        mTabbedActivityTestRule.loadUrl(
                JUnitTestGURLs.SEARCH_URL.getSpec(), LOAD_URL_TOTAL_WAIT_TIME_SECONDS);
        ChromeApplicationTestUtils.fireHomeScreenIntent(mAppContext);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testSessionDidNotContainGoogleSearchPage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                TABBED_SESSION_CONTAINED_GOOGLE_SEARCH_HISTOGRAM, false)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();

        // Histogram record is false when session does not contain SRP.
        // Initial launch of the flaky test runner can result in Chrome being put to sleep so extra
        // records are allowed in case that happens.
        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        mTabbedActivityTestRule.waitForActivityNativeInitializationComplete();
        ChromeApplicationTestUtils.fireHomeScreenIntent(mAppContext);

        histogramWatcher.assertExpected();
    }
}
