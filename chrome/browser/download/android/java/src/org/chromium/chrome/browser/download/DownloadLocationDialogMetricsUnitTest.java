// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static junit.framework.Assert.assertEquals;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.chrome.browser.download.DownloadLocationDialogMetrics.DownloadLocationSuggestionEvent;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit test for {@link DownloadLocationDialogMetrics}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class DownloadLocationDialogMetricsUnitTest {
    private static final String EVENT_METRIC_NAME =
            "MobileDownload.Location.Dialog.Suggestion.Events";
    private static final String SELECTED_METRIC_NAME =
            "MobileDownload.Location.Dialog.SuggestionSelected";

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
    }

    @After
    public void tearDown() {
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testRecordDownloadLocationDialogSuggestionEvent() {
        DownloadLocationDialogMetrics.recordDownloadLocationSuggestionEvent(
                DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        EVENT_METRIC_NAME, DownloadLocationSuggestionEvent.NOT_ENOUGH_SPACE_SHOWN));
    }

    @Test
    public void testRecordDownloadLocationSuggestionChoice() {
        DownloadLocationDialogMetrics.recordDownloadLocationSuggestionChoice(true);
        assertEquals(
                1, ShadowRecordHistogram.getHistogramTotalCountForTesting(SELECTED_METRIC_NAME));
    }
}
