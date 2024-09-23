// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.SystemClock;

import org.chromium.build.annotations.CheckDiscard;

/**
 * Utilities related to timestamps, including the ability to use fake time for tests via
 * FakeTimeTestRule.
 */
public class TimeUtils {
    /**
     * Interval timer using SystemClock.uptimeMillis() (excludes deep sleep).
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Class should get inlined by R8.")
    public static class UptimeMillisTimer {
        private final long mStart = uptimeMillis();

        public long getElapsedMillis() {
            return uptimeMillis() - mStart;
        }
    }

    /**
     * Interval timer using SystemClock.elapsedRealtime() (includes deep sleep).
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Class should get inlined by R8.")
    public static class ElapsedRealtimeMillisTimer {
        private final long mStart = elapsedRealtimeMillis();

        public long getElapsedMillis() {
            return elapsedRealtimeMillis() - mStart;
        }
    }

    /**
     * Interval timer using SystemClock.elapsedRealtimeNanos() (includes deep sleep).
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Class should get inlined by R8.")
    public static class ElapsedRealtimeNanosTimer {
        private final long mStart = elapsedRealtimeNanos();

        public long getElapsedNanos() {
            return elapsedRealtimeNanos() - mStart;
        }
    }

    /**
     * Interval timer using SystemClock.currentThreadTimeMillis() (excludes blocking time).
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Class should get inlined by R8.")
    public static class CurrentThreadTimeMillisTimer {
        private final long mStart = currentThreadTimeMillis();

        public long getElapsedMillis() {
            return currentThreadTimeMillis() - mStart;
        }
    }

    interface FakeClock {
        long uptimeMillis();

        long elapsedRealtimeNanos();

        long currentThreadTimeMillis();

        long currentTimeMillis();
    }

    private TimeUtils() {}

    // Use these in favor of TimeUnit.convert() in order to avoid the overhead of a
    // static-get / static-invoke.
    public static final long SECONDS_PER_MINUTE = 60;
    public static final long SECONDS_PER_HOUR = SECONDS_PER_MINUTE * 60;
    public static final long SECONDS_PER_DAY = SECONDS_PER_HOUR * 24;
    public static final long MILLISECONDS_PER_MINUTE = SECONDS_PER_MINUTE * 1000;
    public static final long MILLISECONDS_PER_DAY = SECONDS_PER_DAY * 1000;
    public static final long MILLISECONDS_PER_YEAR = MILLISECONDS_PER_DAY * 365;

    public static final long NANOSECONDS_PER_MICROSECOND = 1000;
    public static final long NANOSECONDS_PER_MILLISECOND = 1000000;

    // Used by FakeTimeTestRule. Visibility is restricted to ensure tests use the rule, which
    // restores the value to null in its clean-up logic.
    static FakeClock sFakeClock;

    /**
     * Wrapper for System.currentTimeMillis() (milliseconds since the epoch).
     * Can be faked in tests using FakeTimeTestRule.
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Should get inlined by R8.")
    public static long currentTimeMillis() {
        if (sFakeClock != null) {
            return sFakeClock.currentTimeMillis();
        }
        return System.currentTimeMillis();
    }

    /**
     * Wrapper for SystemClock.uptimeMillis() (uptime excluding deep sleep).
     * Can be faked in tests using FakeTimeTestRule.
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Should get inlined by R8.")
    public static long uptimeMillis() {
        if (sFakeClock != null) {
            return sFakeClock.uptimeMillis();
        }
        return SystemClock.uptimeMillis();
    }

    /**
     * Wrapper for SystemClock.elapsedRealtimeNanos() (uptime including deep sleep).
     * Can be faked in tests using FakeTimeTestRule.
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Should get inlined by R8.")
    public static long elapsedRealtimeNanos() {
        if (sFakeClock != null) {
            return sFakeClock.elapsedRealtimeNanos();
        }
        return SystemClock.elapsedRealtimeNanos();
    }

    /**
     * Wrapper for SystemClock.elapsedRealtimeMillis() (uptime including deep sleep).
     * Can be faked in tests using FakeTimeTestRule.
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Should get inlined by R8.")
    public static long elapsedRealtimeMillis() {
        if (sFakeClock != null) {
            return sFakeClock.elapsedRealtimeNanos() / NANOSECONDS_PER_MILLISECOND;
        }
        return SystemClock.elapsedRealtime();
    }

    /**
     * Wrapper for SystemClock.currentThreadTimeMillis() (excludes blocking time).
     * Can be faked in tests using FakeTimeTestRule.
     * See: https://developer.android.com/reference/android/os/SystemClock
     */
    @CheckDiscard("Should get inlined by R8.")
    public static long currentThreadTimeMillis() {
        if (sFakeClock != null) {
            return sFakeClock.currentThreadTimeMillis();
        }
        return SystemClock.currentThreadTimeMillis();
    }
}
