// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.os.Debug;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class to be used with a try-with-resources to record the elapsed time within the try
 * block. Measures time elapsed between instantiation and the call to close using supplied time
 * source.
 */
public class TimingMetric implements AutoCloseable {
    @IntDef({TimeSource.WALL, TimeSource.THREAD})
    @Retention(RetentionPolicy.SOURCE)
    @interface TimeSource {
        int WALL = 0;
        int THREAD = 1;
    }

    @IntDef({TimeDuration.SHORT, TimeDuration.MEDIUM, TimeDuration.LONG})
    @Retention(RetentionPolicy.SOURCE)
    @interface TimeDuration {
        int SHORT = 0;
        int MEDIUM = 1;
        int LONG = 2;
    }

    private final String mMetric;
    private final @TimeSource int mTimeSource;
    private final @TimeDuration int mTimeDuration;

    /**
     * When non-0, holds the timestamp of the instantiation time of this object. Value of 0
     * indicates canceled or already reported metric.
     */
    private long mStartMillis;

    /**
     * Create a new TimingMetric measuring wall time (ie. time experienced by the User) of up to 3
     * minutes.
     *
     * @param metric The name of the histogram to record.
     */
    public static TimingMetric mediumWallTime(String name) {
        return new TimingMetric(name, TimeSource.WALL, TimeDuration.MEDIUM);
    }

    /**
     * Create a new TimingMetric measuring thread time (ie. actual time spent executing the code) of
     * up to 10 seconds.
     *
     * @param metric The name of the histogram to record.
     */
    public static TimingMetric shortThreadTime(String name) {
        return new TimingMetric(name, TimeSource.THREAD, TimeDuration.SHORT);
    }

    /**
     * Construct a new AutoCloseable time measuring metric.
     * In most cases the user should defer to one of the static constructors to instantiate this
     * class.
     *
     * @param metric The name of the histogram to record.
     * @param timeSource The time source to use.
     * @param timeDuration The anticipated duration for this metric.
     */
    /* package */ TimingMetric(
            String metric, @TimeSource int timeSource, @TimeDuration int timeDuration) {
        mMetric = metric;
        mTimeSource = timeSource;
        mTimeDuration = timeDuration;
        mStartMillis = getCurrentTimeMillis();
    }

    @Override
    public void close() {
        // If the start time has been cancel, do not record the histogram.
        if (mStartMillis == 0) return;
        final long measuredTime = getCurrentTimeMillis() - mStartMillis;
        mStartMillis = 0;

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
        mStartMillis = 0;
    }

    /**
     * Query the time source associated with this metric for current time.
     *
     * @return Current time expressed in milliseconds.
     */
    private long getCurrentTimeMillis() {
        switch (mTimeSource) {
            case TimeSource.WALL:
                return SystemClock.uptimeMillis();
            case TimeSource.THREAD:
                return Debug.threadCpuTimeNanos() / 1000000;
        }
        assert false : "unknown time source requested";
        return 0;
    }
}
