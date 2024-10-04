// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.latency_injection;

import org.chromium.base.TimeUtils;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

public final class StartupLatencyInjector {
    private static final String LATENCY_INJECTION_PARAM = "latency_injection_amount_millis";

    private static final int LATENCY_INJECTION_DEFAULT_MILLIS = 0;

    private static final String HISTOGRAM_TOTAL_WAIT_TIME =
            "Startup.Android.MainIconLaunchTotalWaitTime";

    private final Long mBusyWaitDurationMillis;

    /**
     * A cached parameter representing the amount of latency to inject during Clank startup based on
     * experiment configuration.
     */
    public static final IntCachedFieldTrialParameter CLANK_STARTUP_LATENCY_PARAM_MS =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.CLANK_STARTUP_LATENCY_INJECTION,
                    LATENCY_INJECTION_PARAM,
                    LATENCY_INJECTION_DEFAULT_MILLIS);

    public StartupLatencyInjector() {
        mBusyWaitDurationMillis = Long.valueOf(CLANK_STARTUP_LATENCY_PARAM_MS.getValue());
    }

    private boolean isEnabled() {
        return ChromeFeatureList.sClankStartupLatencyInjection.isEnabled();
    }

    public void maybeInjectLatency() {
        if (!isEnabled() || mBusyWaitDurationMillis <= 0) {
            return;
        }

        long startTime = TimeUtils.uptimeMillis();
        busyWait();
        long totalWaitTime = TimeUtils.uptimeMillis() - startTime;
        RecordHistogram.recordMediumTimesHistogram(HISTOGRAM_TOTAL_WAIT_TIME, totalWaitTime);
    }

    private void busyWait() {
        UptimeMillisTimer timer = new UptimeMillisTimer();
        while (mBusyWaitDurationMillis.compareTo(timer.getElapsedMillis()) >= 0);
    }
}
