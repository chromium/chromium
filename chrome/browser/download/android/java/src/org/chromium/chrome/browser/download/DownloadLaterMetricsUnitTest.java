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
import org.chromium.chrome.browser.download.DownloadLaterMetrics.DownloadLaterUiEvent;
import org.chromium.chrome.browser.download.dialogs.DownloadLaterDialogChoice;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit test for {@link DownloadLaterMetrics}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class DownloadLaterMetricsUnitTest {
    private static final String UI_EVENT_METRIC_NAME = "Download.Later.UI.Events";

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
    }

    @After
    public void tearDown() {
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testRecordDownloadLaterUiEvent() {
        DownloadLaterMetrics.recordDownloadLaterUiEvent(
                DownloadLaterUiEvent.DOWNLOAD_LATER_DIALOG_EDIT_CLICKED);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(UI_EVENT_METRIC_NAME,
                        DownloadLaterUiEvent.DOWNLOAD_LATER_DIALOG_EDIT_CLICKED));
    }

    @Test
    public void testRecordDownloadLaterDialogChoice() {
        DownloadLaterMetrics.recordDownloadLaterDialogChoice(
                DownloadLaterDialogChoice.ON_WIFI, true, -1);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Main", DownloadLaterDialogChoice.ON_WIFI));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Main.DataSaverOn",
                        DownloadLaterDialogChoice.ON_WIFI));
        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Main.DataSaverOff",
                        DownloadLaterDialogChoice.ON_WIFI));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        UI_EVENT_METRIC_NAME, DownloadLaterUiEvent.DOWNLOAD_LATER_DIALOG_COMPLETE));

        DownloadLaterMetrics.recordDownloadLaterDialogChoice(
                DownloadLaterDialogChoice.DOWNLOAD_LATER, false, 1024);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Main",
                        DownloadLaterDialogChoice.DOWNLOAD_LATER));
        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Main.DataSaverOn",
                        DownloadLaterDialogChoice.DOWNLOAD_LATER));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Main.DataSaverOff",
                        DownloadLaterDialogChoice.DOWNLOAD_LATER));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.ScheduledDownloadSize", 0));
    }

    @Test
    public void testRecordDownloadHomeChangeScheduleChoice() {
        DownloadLaterMetrics.recordDownloadHomeChangeScheduleChoice(
                DownloadLaterDialogChoice.DOWNLOAD_LATER);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.DownloadHome",
                        DownloadLaterDialogChoice.DOWNLOAD_LATER));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(UI_EVENT_METRIC_NAME,
                        DownloadLaterUiEvent.DOWNLOAD_HOME_CHANGE_SCHEDULE_COMPLETE));
    }

    @Test
    public void testRecordInfobarChangeScheduleChoice() {
        DownloadLaterMetrics.recordInfobarChangeScheduleChoice(
                DownloadLaterDialogChoice.DOWNLOAD_NOW);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Download.Later.UI.DialogChoice.Infobar",
                        DownloadLaterDialogChoice.DOWNLOAD_NOW));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(UI_EVENT_METRIC_NAME,
                        DownloadLaterUiEvent.DOWNLOAD_INFOBAR_CHANGE_SCHEDULE_COMPLETE));
    }
}
