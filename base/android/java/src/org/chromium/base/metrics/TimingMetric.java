// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.time.Timer;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * A class to be used with a try-with-resources to record the elapsed time within the try
 * block. Measures time elapsed between instantiation and the call to close using supplied time
 * source.
 */
public class TimingMetric implements AutoCloseable {
    @IntDef({TimeDuration.SHORT, TimeDuration.MEDIUM, TimeDuration.LONG})
    @Retention(RetentionPolicy.SOURCE)
    @interface TimeDuration {
        int SHORT = 0;
        int MEDIUM = 1;
        int LONG = 2;
    }

    private final String mMetric;
    private final @TimeDuration int mTimeDuration;
    private @Nullable Timer mTimer;

    /**
     * Create a new TimingMetric measuring wall time (ie. time experienced by the user) of
     * up to 10 seconds.
     *
     * @param metric The name of the histogram to record.
     */
    public static TimingMetric shortWallTime(String name) {
        return new TimingMetric(name, Timer.forUpTime(), TimeDuration.SHORT);
    }

    /**
     * Create a new TimingMetric measuring wall time (ie. time experienced by the user) of up to 3
     * minutes.
     *
     * @param metric The name of the histogram to record.
     */
    public static TimingMetric mediumWallTime(String name) {
        return new TimingMetric(name, Timer.forUpTime(), TimeDuration.MEDIUM);
    }

    /**
     * Create a new TimingMetric measuring thread time (ie. actual time spent executing the code) of
     * up to 10 seconds.
     *
     * @param metric The name of the histogram to record.
     */
    public static TimingMetric shortThreadTime(String name) {
        return new TimingMetric(name, Timer.forCpuTime(), TimeDuration.SHORT);
    }

    /**
     * Construct a new AutoCloseable time measuring metric.
     * In most cases the user should defer to one of the static constructors to instantiate this
     * class.
     *
     * @param metric The name of the histogram to record.
     * @param timer The timer to use.
     * @param timeDuration The anticipated duration for this metric.
     */
    /* package */ TimingMetric(
            @NonNull String metric, @NonNull Timer timer, @TimeDuration int timeDuration) {
        mMetric = metric;
        mTimer = timer;
        mTimeDuration = timeDuration;
    }

    @Override
    public void close() {
        if (mTimer == null) return;

        final long measuredTime = mTimer.getElapsedTime(TimeUnit.MILLISECONDS);
        mTimer = null;

        switch (mTimeDuration) {
            case TimeDuration.SHORT:
                RecordHistogram.recordTimesHistogram(mMetric, measuredTime);
                break;
            case TimeDuration.MEDIUM:
                RecordHistogram.recordMediumTimesHistogram(mMetric, measuredTime);
                break;
            case TimeDuration.LONG:
                RecordHistogram.recordLongTimesHistogram(mMetric, measuredTime);
                break;
        }
    }

    /**
     * Cancel the measurement.
     */
    public void cancel() {
        mTimer = null;
    }
}
