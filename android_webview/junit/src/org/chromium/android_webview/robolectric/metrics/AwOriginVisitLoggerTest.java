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

import org.chromium.android_webview.metrics.AwOriginVisitLogger;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@link AwOriginVisitLogger}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwOriginVisitLoggerTest {
    private static final long MILLIS_PER_DAY = TimeUtils.SECONDS_PER_DAY * 1000;
    private static final long ORIGIN_HASH_A = 1778564728L;
    private static final long ORIGIN_HASH_B = 3169559102L;
    private static final long ORIGIN_HASH_C = 1894809809L;
    private static final String HISTOGRAM_NAME = "Android.WebView.OriginsVisited";

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOneVisit() {
        // Visiting one distinct origin does not trigger histogram recording.
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // Next day when an origin is visited the previous visits are counted and recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_B);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testTwoVisits() {
        // Visiting two distinct origins does not trigger histogram recording.
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_B);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // Next day when an origin is visited the previous visits are counted and recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 2));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRepeatVisits() {
        // Visiting one distinct origin repeatedly does not trigger histogram recording.
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // Next day when an origin is visited the previous visits are counted and recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_B);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMultipleDaysHavePassed() {
        // Visiting one distinct origin does not trigger histogram recording.
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // It does not matter how much time passes, it could be multiple days. The logic is that the
        // first time any origin is visited after a day has ended, the histogram is recorded.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY * 7);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_B);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMultipleDaysOfUsage() {
        // Visit one origin.
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        assertEquals(0, getHistogramTotalCountForTesting(HISTOGRAM_NAME));

        // One day passes, and three origins are visited.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_B);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_C);
        assertEquals(1, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));

        // A second day passes, and one origin is visited.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        assertEquals(2, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 3));

        // A third day passes, and two origins are visited.
        mFakeTimeTestRule.advanceMillis(MILLIS_PER_DAY);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_A);
        AwOriginVisitLogger.logOriginVisit(ORIGIN_HASH_B);
        assertEquals(3, getHistogramTotalCountForTesting(HISTOGRAM_NAME));
        assertEquals(2, getHistogramValueCountForTesting(HISTOGRAM_NAME, 1));
        assertEquals(1, getHistogramValueCountForTesting(HISTOGRAM_NAME, 3));
    }
}
