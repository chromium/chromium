// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Implementation for a provider of elapsed time, useful for general purpose interval timing.
 * Guarantees that returned values are monotonically non-decreasing.
 *
 * Currently all time is internally represented in nanoseconds:
 * - 1 year is about 3.2e+16ns
 * - 2^63-1 is about 9.2e+18
 * indicating capacity of 292 years in (signed) long.
 * https://google.com/search?q=%282%5E63-1%29ns+in+years
 */
public class Timer {
    /**
     * Describes available sources of the current time.
     * Sources must be monotonically non-decreasing.
     */
    @IntDef({TimerType.TEST_TIME, TimerType.UP_TIME, TimerType.ELAPSED_REAL_TIME,
            TimerType.CPU_TIME})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface TimerType {
        // Used for testing purposes only. See {@link TestTimer}.
        int TEST_TIME = 0;

        // Use the Android SystemClock's uptimeMillis to track elapsed time excluding deep sleep.
        int UP_TIME = 1;

        // Use the Android's SystemClock to track elapsed real time at an internal resolution of
        // nanoseconds. This is a good default choice for interval timing.
        int ELAPSED_REAL_TIME = 2;

        // Use the Android SystemClock's currentThreadTimeMillis to track CPU thread time. This is
        // typically *not* what you want for interval timing, and should only be used if you're
        // sure what you're measuring is CPU bound.
        int CPU_TIME = 3;
    }

    /** The source of the current time. */
    protected final @TimerType int mTimerType;

    /**
     * Starting time of the timer. This is equal to 0 if the timer has never been started, and
     * otherwise is equal to the time at which the timer was most recently started.
     */
    private long mStartTimeNano;

    /**
     * Acquire timer using Android SystemClock's uptimeMillis to track elapsed time excluding deep
     * sleep.
     */
    public static @NonNull Timer forUpTime() {
        return new Timer(TimerType.UP_TIME);
    }

    /**
     * Acquire timer using Android's SystemClock to track elapsed real time at an internal
     * resolution of nanoseconds. This is a good default choice for interval timing.
     */
    public static @NonNull Timer forElapsedRealTime() {
        return new Timer(TimerType.ELAPSED_REAL_TIME);
    }

    /**
     * Use the Android SystemClock's currentThreadTimeMillis to track CPU thread time. This is
     * typically *not* what you want for interval timing, and should only be used if you're
     * sure what you're measuring is CPU bound.
     */
    public static @NonNull Timer forCpuTime() {
        return new Timer(TimerType.CPU_TIME);
    }

    /**
     * Constructs and starts a new Timer.
     * Please use any of the factory methods listed above to create a new timer.
     *
     * @param timerType Type of timer to use.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected Timer(@TimerType int timerType) {
        mTimerType = timerType;
        restart();
    }

    /** Restarts the timer, moving the beginning of the measured interval to current time. */
    public void restart() {
        mStartTimeNano = getCurrentTimeNano();
    }

    /**
     * Returns the elapsed time. This is the delta between the current time and the timer was last
     * (re)started. The result is in terms of the desired TimeUnit, rounding down.
     */
    public long getElapsedTime(@NonNull TimeUnit timeUnit) {
        return timeUnit.convert(getCurrentTimeNano() - mStartTimeNano, TimeUnit.NANOSECONDS);
    }

    /**
     * Returns the time as read from the requested time source, expressed in TimeUnit.NANOSECONDS.
     */
    protected long getCurrentTimeNano() {
        switch (mTimerType) {
            case TimerType.UP_TIME:
                return TimeUnit.NANOSECONDS.convert(
                        SystemClock.uptimeMillis(), TimeUnit.MILLISECONDS);

            case TimerType.ELAPSED_REAL_TIME:
                return SystemClock.elapsedRealtimeNanos();

            case TimerType.CPU_TIME:
                return TimeUnit.NANOSECONDS.convert(
                        SystemClock.currentThreadTimeMillis(), TimeUnit.MILLISECONDS);

            case TimerType.TEST_TIME:
                assert false : "Do not use TEST_TIME directly; use TestTimer";
        }

        assert false : "Unknown/invalid timer type " + mTimerType;
        return 0;
    }
}
