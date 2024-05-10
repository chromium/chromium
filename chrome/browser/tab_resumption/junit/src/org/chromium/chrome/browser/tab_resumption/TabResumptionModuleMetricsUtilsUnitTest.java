// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;

/** Unit tests for {@link TabResumptionModuleMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleMetricsUtilsUnitTest {
    @Test
    @SmallTest
    public void testRecordSalientImageAvailability() {
        String histogramName = "MagicStack.Clank.TabResumption.IsSalientImageAvailable";

        var histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, true).build();
        TabResumptionModuleMetricsUtils.recordSalientImageAvailability(true);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, false).build();
        TabResumptionModuleMetricsUtils.recordSalientImageAvailability(false);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecord() {
        String histogramName = "MagicStack.Clank.TabResumption.SeeMoreLinkClicked";
        @ModuleShowConfig int config = ModuleShowConfig.DOUBLE_TILE_LOCAL_FOREIGN;
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectIntRecord(histogramName, config).build();
        TabResumptionModuleMetricsUtils.recordSeeMoreLinkClicked(config);
        histogramWatcher.assertExpected();
    }
}
