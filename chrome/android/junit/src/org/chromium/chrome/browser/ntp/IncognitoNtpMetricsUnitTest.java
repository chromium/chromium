// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Unit tests for {@link IncognitoNtpMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoNtpMetricsUnitTest {
    private IncognitoNtpMetrics mIncognitoNtpMetrics;

    @Before
    public void setUp() {
        mIncognitoNtpMetrics = new IncognitoNtpMetrics();
    }

    @Test
    public void testRecordTimeToFirstNavigation() {
        final HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpMetrics.HISTOGRAM_TIME_TO_FIRST_NAVIGATION);

        mIncognitoNtpMetrics.markNtpLoaded();
        mIncognitoNtpMetrics.recordNavigatedAway();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordOnlyOnce() {
        final HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpMetrics.HISTOGRAM_TIME_TO_FIRST_NAVIGATION);

        mIncognitoNtpMetrics.markNtpLoaded();
        mIncognitoNtpMetrics.recordNavigatedAway();
        mIncognitoNtpMetrics.recordNavigatedAway();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordOnlyOnce_withMultipleNtpLoadedEvents() {
        final HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpMetrics.HISTOGRAM_TIME_TO_FIRST_NAVIGATION);

        mIncognitoNtpMetrics.markNtpLoaded();
        mIncognitoNtpMetrics.markNtpLoaded();
        mIncognitoNtpMetrics.recordNavigatedAway();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testNoRecordWithoutNtpLoaded() {
        final HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(IncognitoNtpMetrics.HISTOGRAM_TIME_TO_FIRST_NAVIGATION)
                        .build();

        mIncognitoNtpMetrics.recordNavigatedAway();

        histogramWatcher.assertExpected();
    }
}
