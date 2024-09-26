// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.TimeUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class to be used with a try-with-resources to record the elapsed time within the try
 * block. Measures time elapsed between instantiation and the call to close using supplied time
 * source.
 */
public class TimingMetric implements AutoCloseable {
    @IntDef({TimerType.SHORT_UPTIME, TimerType.MEDIUM_UPTIME, TimerType.SHORT_THREAD_TIME})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TimerType {
        int SHORT_UPTIME = 0;
        int MEDIUM_UPTIME = 1;
        int SHORT_THREAD_TIME = 2;
    }

    private final String mMetricName;
    private final @TimerType int mTimerType;
    private long mStartTime;

    /**
     * Create a new TimingMetric measuring wall time (ie. time experienced by the user) of up to 10
     * seconds.
     *
     * @param metricName The name of the histogram to record.
     */
    public static TimingMetric shortUptime(@NonNull String metricName) {
        TimingMetric ret = new TimingMetric(metricName, TimerType.SHORT_UPTIME);
        ret.mStartTime = TimeUtils.uptimeMillis();
        return ret;
    }

    /**
     * Create a new TimingMetric measuring wall time (ie. time experienced by the user) of up to 3
     * minutes.
     *
     * @param metricName The name of the histogram to record.
     */
    public static TimingMetric mediumUptime(@NonNull String metricName) {
        TimingMetric ret = new TimingMetric(metricName, TimerType.MEDIUM_UPTIME);
        ret.mStartTime = TimeUtils.uptimeMillis();
        return ret;
    }

    /**
     * Create a new TimingMetric measuring thread time (ie. actual time spent executing the code) of
     * up to 10 seconds.
     *
     * @param metricName The name of the histogram to record.
     */
    public static TimingMetric shortThreadTime(@NonNull String metricName) {
        TimingMetric ret = new TimingMetric(metricName, TimerType.SHORT_THREAD_TIME);
        ret.mStartTime = TimeUtils.currentThreadTimeMillis();
        return ret;
    }

    private TimingMetric(String metricName, @TimerType int timerType) {
        mMetricName = metricName;
        mTimerType = timerType;
    }

    @Override
    public void close() {
        String metricName = mMetricName;
        long startTime = mStartTime;
        if (startTime == 0) return;
        mStartTime = 0;

        switch (mTimerType) {
            case TimerType.SHORT_UPTIME:
                RecordHistogram.recordTimesHistogram(
                        metricName, TimeUtils.uptimeMillis() - startTime);
                break;
            case TimerType.MEDIUM_UPTIME:
                RecordHistogram.recordMediumTimesHistogram(
                        metricName, TimeUtils.uptimeMillis() - startTime);
                break;
            case TimerType.SHORT_THREAD_TIME:
                RecordHistogram.recordTimesHistogram(
                        metricName, TimeUtils.currentThreadTimeMillis() - startTime);
                break;
        }
    }
}
