// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Unit tests for {@link ReadAloudMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadAloudMetricsUnitTest {
    @Test
    @SmallTest
    public void testRecordIsPageReadable() {
        final String histogramName = ReadAloudMetrics.READABILITY_SUCCESS;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ReadAloudMetrics.recordIsPageReadable(true);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        ReadAloudMetrics.recordIsPageReadable(false);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordUserEligibility() {
        final String histogramName = ReadAloudMetrics.IS_USER_ELIGIBLE;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, true);
        ReadAloudMetrics.recordIsUserEligible(true);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, false);
        ReadAloudMetrics.recordIsUserEligible(false);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordIneligibilityReason() {
        final String histogramName = ReadAloudMetrics.INELIGIBILITY_REASON;

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, 1);
        ReadAloudMetrics.recordIneligibilityReason(1);
        histogram.assertExpected();

        histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, 4);
        ReadAloudMetrics.recordIneligibilityReason(4);
        histogram.assertExpected();
    }
}
