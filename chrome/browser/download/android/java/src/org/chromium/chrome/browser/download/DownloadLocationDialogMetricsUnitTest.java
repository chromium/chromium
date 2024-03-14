// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics.DownloadLocationSuggestionEvent;

/** Unit test for {@link DownloadLocationDialogMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadLocationDialogMetricsUnitTest {
    private static final String EVENT_METRIC_NAME =
            "MobileDownload.Location.Dialog.Suggestion.Events";
    private static final String SELECTED_METRIC_NAME =
            "MobileDownload.Location.Dialog.SuggestionSelected";

    @Test
    public void testRecordDownloadLocationDialogSuggestionEvent() {
        DownloadLocationDialogMetrics.recordDownloadLocationSuggestionEvent(
                DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN);
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        EVENT_METRIC_NAME, DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN));
    }

    @Test
    public void testRecordDownloadLocationSuggestionChoice() {
        DownloadLocationDialogMetrics.recordDownloadLocationSuggestionChoice(true);
        assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(SELECTED_METRIC_NAME));
    }
}
