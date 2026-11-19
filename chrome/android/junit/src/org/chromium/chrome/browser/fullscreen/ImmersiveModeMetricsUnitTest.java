// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.fullscreen.ImmersiveModeMetrics.ImmersiveModeEntryMethod;

/** Unit tests for {@link ImmersiveModeMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersiveModeMetricsUnitTest {
    @Test
    public void testRecordEntryMethod() {
        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                ImmersiveModeMetrics.HISTOGRAM_ENTRY_METHOD,
                                ImmersiveModeEntryMethod.APP_MENU_FULLSCREEN_BUTTON,
                                ImmersiveModeEntryMethod.KEYBOARD_SHORTCUT)
                        .build()) {
            ImmersiveModeMetrics.recordEntryMethod(
                    ImmersiveModeEntryMethod.APP_MENU_FULLSCREEN_BUTTON);
            ImmersiveModeMetrics.recordEntryMethod(ImmersiveModeEntryMethod.KEYBOARD_SHORTCUT);
        }
    }

    @Test
    public void testRecordSessionMetrics() {
        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 4)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 7)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 12345)
                        .build()) {
            ImmersiveModeMetrics.recordSessionMetrics(
                    /* navigationsCount= */ 4, /* tabSwitchesCount= */ 7, /* durationMs= */ 12345L);
        }
    }

    @Test
    public void testRecordSessionMetrics_ZeroAndOverflowCounts() {
        // Values >= 100 in recordCount100Histogram fall into the overflow bucket (100).
        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 0, 150)
                        .expectIntRecords(
                                ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 0, 200)
                        .expectIntRecords(
                                ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 0, 3600000)
                        .build()) {
            ImmersiveModeMetrics.recordSessionMetrics(
                    /* navigationsCount= */ 0, /* tabSwitchesCount= */ 0, /* durationMs= */ 0L);
            ImmersiveModeMetrics.recordSessionMetrics(
                    /* navigationsCount= */ 150,
                    /* tabSwitchesCount= */ 200,
                    /* durationMs= */ 3600000L);
        }
    }
}
