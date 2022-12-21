// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * Logs tab switching latency metrics
 */
public class TabSwitchMetrics {
    // TODO(dtrainor, simonb): Make these non-static so we don't break if we have multiple instances
    // of chrome running.  Also investigate how this affects document mode.
    private static long sTabSwitchStartTime;
    private static @TabSelectionType int sTabSelectionType;
    private static boolean sTabSwitchLatencyMetricRequired;

    /**
     * Register the start of tab switch latency timing. Called when setIndex() indicates a tab
     * switch event.
     * @param type The type of action that triggered the tab selection.
     */
    public static void startTabSwitchLatencyTiming(final @TabSelectionType int type) {
        sTabSwitchStartTime = SystemClock.uptimeMillis();
        sTabSelectionType = type;
        sTabSwitchLatencyMetricRequired = false;
    }

    /**
     * Should be called a visible {@link Tab} gets a frame to render in the browser process.
     * If we don't get this call, we ignore requests to
     * {@link #flushActualTabSwitchLatencyMetric()}.
     */
    public static void setActualTabSwitchLatencyMetricRequired() {
        if (sTabSwitchStartTime <= 0) return;
        sTabSwitchLatencyMetricRequired = true;
    }

    /**
     * Flush the latency metric if called after the indication that a frame is ready.
     */
    public static void flushActualTabSwitchLatencyMetric() {
        if (sTabSwitchStartTime <= 0 || !sTabSwitchLatencyMetricRequired) return;
        flushTabSwitchLatencyMetric();

        sTabSwitchStartTime = 0;
        sTabSwitchLatencyMetricRequired = false;
    }

    private static void flushTabSwitchLatencyMetric() {
        if (sTabSwitchStartTime <= 0) return;
        final long ms = SystemClock.uptimeMillis() - sTabSwitchStartTime;
        String baseHistogram;
        switch (sTabSelectionType) {
            case TabSelectionType.FROM_CLOSE:
                baseHistogram = "Tabs.SwitchFromCloseLatency";
                break;
            case TabSelectionType.FROM_EXIT:
                baseHistogram = "Tabs.SwitchFromExitLatency";
                break;
            case TabSelectionType.FROM_NEW:
                baseHistogram = "Tabs.SwitchFromNewLatency";
                break;
            case TabSelectionType.FROM_USER:
                baseHistogram = "Tabs.SwitchFromUserLatency";
                break;
            default:
                return;
        }
        RecordHistogram.recordTimesHistogram(baseHistogram + "_Actual", ms);
    }
}
