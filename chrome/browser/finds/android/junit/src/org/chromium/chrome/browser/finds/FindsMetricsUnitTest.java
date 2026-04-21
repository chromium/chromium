// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.finds.FindsMetrics.FindsOptInEvent;

/** Unit tests for {@link FindsMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FindsMetricsUnitTest {
    @Test
    public void testRecordOptInShown() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsOptInEvent.SHOWN);
        FindsMetrics.recordOptInShown();
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptInAccepted_FirstTime() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsOptInEvent.ACCEPTED_FIRST_TIME);
        FindsMetrics.recordOptInAccepted(/* firstTime= */ true);
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptInAccepted_NotFirstTime() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FindsMetrics.OPT_IN_HISTOGRAM)
                        .build();
        FindsMetrics.recordOptInAccepted(/* firstTime= */ false);
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptOutClicked() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsOptInEvent.DECLINED);
        FindsMetrics.recordOptOutClicked();
        histogram.assertExpected();
    }

    @Test
    public void testRecordSnackbarActionClicked() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsOptInEvent.SNACKBAR_ACTION_CLICKED);
        FindsMetrics.recordSnackbarActionClicked();
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptInDismissed() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FindsMetrics.OPT_IN_HISTOGRAM, FindsOptInEvent.DISMISSED);
        FindsMetrics.recordOptInDismissed();
        histogram.assertExpected();
    }
}
