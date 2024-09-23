// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.metrics;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.metrics.RecordHistogram.getHistogramTotalCountForTesting;
import static org.chromium.base.metrics.RecordHistogram.getHistogramValueCountForTesting;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.metrics.AwSiteVisitLogger;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;

/** Unit tests for {@link AwSiteVisitLogger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwSiteVisitLoggerTest {
    // We test for 1 millisecond after a week has passed as our logic checks are
    // for after a week has passed
    private static final long MILLIS_PER_WEEK = (TimeUtils.SECONDS_PER_DAY * 7) * 1000 + 1;
    private static final long SITE_HASH_A = 1778564728L;
    private static final long SITE_HASH_B = 3169559102L;
    private static final long SITE_HASH_C = 1894809809L;
    private static final String HISTOGRAM_NAME = "Android.WebView.SitesVisitedWeekly";
    private static final String RELATED_HISTOGRAM_NAME =
            "Android.WebView.RelatedSitesVisitedWeekly";

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOneVisit() {
        // Visiting one distinct site does not trigger histogram recording.
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // Next week when an site is visited the previous visits are counted and recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK);
        AwSiteVisitLogger.logVisit(SITE_HASH_B, false);
        assertEquals(0, getHistogramTotalCountForTesting(RELATED_HISTOGRAM_NAME));
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTwoVisits() {
        HistogramWatcher relatedExpectation =
                HistogramWatcher.newSingleRecordWatcher(RELATED_HISTOGRAM_NAME, 1);

        // Visiting two distinct sites does not trigger histogram recording.
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        AwSiteVisitLogger.logVisit(SITE_HASH_B, true);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // Next week when an site is visited the previous visits are counted and recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK);
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 2));

        relatedExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRepeatVisits() {
        // Visiting one distinct site repeatedly does not trigger histogram recording.
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // Next day when an site is visited the previous visits are counted and recorded.
        // Only one unique visit will be recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK);
        AwSiteVisitLogger.logVisit(SITE_HASH_B, false);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMultipleWeeksHavePassed() {
        // Visiting one distinct site does not trigger histogram recording.
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // It does not matter how much time passes, it could be multiple days. The logic is that the
        // first time any site is visited after a week has passed, the histogram is recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK * 3);
        AwSiteVisitLogger.logVisit(SITE_HASH_B, false);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMultipleWeeksOfUsage() {
        // Visit one site.
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // One week passes, and three sites are visited.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK);
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        AwSiteVisitLogger.logVisit(SITE_HASH_B, false);
        AwSiteVisitLogger.logVisit(SITE_HASH_C, false);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));

        // A second week passes, and one site is visited.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK);
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        assertEquals(2, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 3));

        // A third week passes, and two sites are visited.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_WEEK);
        AwSiteVisitLogger.logVisit(SITE_HASH_A, false);
        AwSiteVisitLogger.logVisit(SITE_HASH_B, false);
        assertEquals(3, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(2, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 3));
    }
}
