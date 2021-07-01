// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.jank_tracker;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Sends Android jank metrics to native to be recorded using UMA.
 */
@JNINamespace("base::android")
public class JankMetricUMARecorder {
    public static void recordJankMetricsToUMA(JankMetrics metric, @JankScenario int scenario) {
        if (metric == null) {
            return;
        }

        JankMetricUMARecorderJni.get().recordJankMetrics(scenarioToString(scenario),
                metric.durationsNs, metric.jankBurstsNs, metric.skippedFrames);
    }

    // Convert an enum value to string to use as an UMA histogram name, changes to strings should be
    // reflected in android/histograms.xml.
    private static String scenarioToString(@JankScenario int scenario) {
        switch (scenario) {
            case JankScenario.PERIODIC_REPORTING:
                return "Total";
            case JankScenario.OMNIBOX_FOCUS:
                return "OmniboxFocus";
            case JankScenario.NEW_TAB_PAGE:
                return "NewTabPage";
            case JankScenario.STARTUP:
                return "Startup";
            case JankScenario.TAB_SWITCHER:
                return "TabSwitcher";
            case JankScenario.OPEN_LINK_IN_NEW_TAB:
                return "OpenLinkInNewTab";
            case JankScenario.START_SURFACE_HOMEPAGE:
                return "StartSurfaceHomepage";
            case JankScenario.START_SURFACE_TAB_SWITCHER:
                return "StartSurfaceTabSwitcher";
            default:
                throw new IllegalArgumentException("Invalid scenario value");
        }
    }

    @NativeMethods
    interface Natives {
        void recordJankMetrics(
                String scenarioName, long[] durationsNs, long[] jankBurstsNs, int missedFrames);
    }
}
