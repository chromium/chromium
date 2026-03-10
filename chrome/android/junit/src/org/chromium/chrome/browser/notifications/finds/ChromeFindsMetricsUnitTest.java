// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.finds;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.notifications.finds.ChromeFindsMetrics.ChromeFindsOptInEvent;

/** Unit tests for {@link ChromeFindsMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeFindsMetricsUnitTest {
    @Test
    public void testRecordOptInShown() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM, ChromeFindsOptInEvent.SHOWN);
        ChromeFindsMetrics.recordOptInShown();
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptInAccepted_FirstTime() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsOptInEvent.ACCEPTED_FIRST_TIME);
        ChromeFindsMetrics.recordOptInAccepted(/* firstTime= */ true);
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptInAccepted_ReOptIn() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsOptInEvent.ACCEPTED_RE_OPT_IN);
        ChromeFindsMetrics.recordOptInAccepted(/* firstTime= */ false);
        histogram.assertExpected();
    }

    @Test
    public void testRecordOptOutClicked() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM, ChromeFindsOptInEvent.DECLINED);
        ChromeFindsMetrics.recordOptOutClicked();
        histogram.assertExpected();
    }

    @Test
    public void testRecordSnackbarActionClicked() {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeFindsMetrics.OPT_IN_HISTOGRAM,
                        ChromeFindsOptInEvent.SNACKBAR_ACTION_CLICKED);
        ChromeFindsMetrics.recordSnackbarActionClicked();
        histogram.assertExpected();
    }
}
