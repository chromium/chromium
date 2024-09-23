// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link MerchantTrustMessageContext}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustMetricsTest {

    private MerchantTrustMetrics mMetrics;

    @Before
    public void setUp() {
        mMetrics = new MerchantTrustMetrics();
    }

    @Test
    public void testRecordingMessageImpact_ZeroNavigation() {
        mMetrics.startRecordingMessageImpact("fakeHost1", 4.7);
        mMetrics.updateRecordingMessageImpact("fakeHost2");

        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM
                                + ".RatingAboveFourPointFive"),
                equalTo(1));

        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM, 0),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM
                                + ".RatingAboveFourPointFive",
                        0),
                equalTo(1));
    }

    @Test
    public void testRecordingMessageImpact_TwoNavigations() {
        mMetrics.startRecordingMessageImpact("fakeHost1", 4.3);
        mMetrics.updateRecordingMessageImpact("fakeHost1");
        mMetrics.updateRecordingMessageImpact("fakeHost1");
        mMetrics.finishRecordingMessageImpact();

        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_BROWSING_TIME_HISTOGRAM
                                + ".RatingAboveFour"),
                equalTo(1));

        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM, 2),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(
                        MerchantTrustMetrics.MESSAGE_IMPACT_NAVIGATION_COUNT_HISTOGRAM
                                + ".RatingAboveFour",
                        2),
                equalTo(1));
    }
}
