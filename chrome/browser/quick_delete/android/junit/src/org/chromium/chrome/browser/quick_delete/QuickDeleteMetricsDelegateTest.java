// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BlockJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataAction;

import java.util.Arrays;
import java.util.List;

/**
 * JUnit tests of the class {@link QuickDeleteMetricsDelegate}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BlockJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class QuickDeleteMetricsDelegateTest {
    /**
     * Class to parameterize the params for {@link
     * QuickDeleteMetricsDelegateTest.testRecordHistogram}.
     */
    public static class MethodParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.PrivacyQuickDelete.MENU_ITEM_CLICKED)
                            .name("MenuItem"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.PrivacyQuickDelete.DELETE_CLICKED)
                            .name("Delete"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.PrivacyQuickDelete.CANCEL_CLICKED)
                            .name("Cancel"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.PrivacyQuickDelete
                                            .DIALOG_DISMISSED_IMPLICITLY)
                            .name("Dismissed"));
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(MethodParams.class)
    public void testRecordHistogram(
            @QuickDeleteMetricsDelegate.PrivacyQuickDelete int mPrivacyQuickDeleteMetric) {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Privacy.QuickDelete", mPrivacyQuickDeleteMetric, 1)
                        .build();

        QuickDeleteMetricsDelegate.recordHistogram(mPrivacyQuickDeleteMetric);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordClearBrowsingDataActionHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Privacy.ClearBrowsingData.Action",
                                ClearBrowsingDataAction.QUICK_DELETE_LAST15_MINUTES, 1)
                        .build();

        QuickDeleteMetricsDelegate.recordHistogram(
                QuickDeleteMetricsDelegate.PrivacyQuickDelete.DELETE_CLICKED);

        histogramWatcher.assertExpected();
    }
}
