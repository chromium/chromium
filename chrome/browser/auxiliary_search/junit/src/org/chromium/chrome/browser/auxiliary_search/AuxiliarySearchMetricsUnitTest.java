// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link AuxiliarySearchMetrics} */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchMetricsUnitTest {

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE})
    public void testRecordDonationCount() {
        int[] counts = new int[AuxiliarySearchEntryType.MAX_VALUE + 1];
        counts[AuxiliarySearchEntryType.TAB] = 1;
        counts[AuxiliarySearchEntryType.CUSTOM_TAB] = 2;

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Search.AuxiliarySearch.DonationCount.Tabs", 1)
                        .expectIntRecord("Search.AuxiliarySearch.DonationCount.CustomTabs", 2)
                        .expectAnyRecordTimes("Search.AuxiliarySearch.DonationCount.TopSites", 0)
                        .build();

        AuxiliarySearchMetrics.recordDonationCount(counts);
        histogramWatcher.assertExpected();
    }
}
