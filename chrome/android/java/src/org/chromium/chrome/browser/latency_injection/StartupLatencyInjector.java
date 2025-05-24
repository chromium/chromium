// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.latency_injection;

import org.chromium.base.TimeUtils;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

@NullMarked
public final class StartupLatencyInjector {
    private static final String HISTOGRAM_TOTAL_WAIT_TIME =
            "Startup.Android.MainIconLaunchTotalWaitTime";

    private final Long mBusyWaitDurationMillis;

    public StartupLatencyInjector() {
        mBusyWaitDurationMillis =
                Long.valueOf(ChromeFeatureList.sClankStartupLatencyInjectionAmountMs.getValue());
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
        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                HISTOGRAM_TOTAL_WAIT_TIME, totalWaitTime);
    }

    private void busyWait() {
        UptimeMillisTimer timer = new UptimeMillisTimer();
        while (mBusyWaitDurationMillis.compareTo(timer.getElapsedMillis()) >= 0)
            ;
    }
}
