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
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;

import java.util.Arrays;
import java.util.List;

/** JUnit tests of the class {@link QuickDeleteMetricsDelegate}. */
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
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.MENU_ITEM_CLICKED)
                            .name("MenuItem"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED)
                            .name("Delete"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED)
                            .name("Cancel"),
                    new ParameterSet()
                            .value(
                                    QuickDeleteMetricsDelegate.QuickDeleteAction
                                            .DIALOG_DISMISSED_IMPLICITLY)
                            .name("Dismissed"),
                    new ParameterSet()
                            .value(
                                    QuickDeleteMetricsDelegate.QuickDeleteAction
                                            .TAB_SWITCHER_MENU_ITEM_CLICKED)
                            .name("TabSwitcherMenuItem"),
                    new ParameterSet()
                            .value(
                                    QuickDeleteMetricsDelegate.QuickDeleteAction
                                            .MORE_OPTIONS_CLICKED)
                            .name("MoreOptions"),
                    new ParameterSet()
                            .value(
                                    QuickDeleteMetricsDelegate.QuickDeleteAction
                                            .MY_ACTIVITY_LINK_CLICKED)
                            .name("MyActivity"),
                    new ParameterSet()
                            .value(
                                    QuickDeleteMetricsDelegate.QuickDeleteAction
                                            .SEARCH_HISTORY_LINK_CLICKED)
                            .name("SearchHistory"),
                    new ParameterSet()
                            .value(
                                    QuickDeleteMetricsDelegate.QuickDeleteAction
                                            .LAST_15_MINUTES_SELECTED)
                            .name("Last15Minutes"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_HOUR_SELECTED)
                            .name("LastHour"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_DAY_SELECTED)
                            .name("LastDay"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_WEEK_SELECTED)
                            .name("LastWeek"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.FOUR_WEEKS_SELECTED)
                            .name("FourWeeks"),
                    new ParameterSet()
                            .value(QuickDeleteMetricsDelegate.QuickDeleteAction.ALL_TIME_SELECTED)
                            .name("AllTime"));
        }
    }

    @Test
    @SmallTest
    @UseMethodParameter(MethodParams.class)
    public void testRecordHistogram(
            @QuickDeleteMetricsDelegate.QuickDeleteAction int quickDeleteAction) {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        QuickDeleteMetricsDelegate.HISTOGRAM_NAME, quickDeleteAction);

        QuickDeleteMetricsDelegate.recordHistogram(quickDeleteAction);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordDeleteBrowsingDataActionHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action", DeleteBrowsingDataAction.QUICK_DELETE);
        QuickDeleteMetricsDelegate.recordHistogram(
                QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);

        histogramWatcher.assertExpected();
    }
}
