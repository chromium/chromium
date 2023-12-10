// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.GuardedBy;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.TimeUtils.FakeClock;

/** Causes fake times to be used in TimeUtils. */
public class FakeTimeTestRule implements TestRule {
    private final Object mLock = new Object();

    // Milliseconds since booted, excluding deep sleep.
    @GuardedBy("mLock")
    private long mUptimeMillis;

    // Nanoseconds since booted, including deep sleep.
    @GuardedBy("mLock")
    private long mElapsedRealtimeNanos;

    // Per-thread CPU time.
    @GuardedBy("mLock")
    private ThreadLocal<Long> mThreadTimes;

    // epoch time.
    @GuardedBy("mLock")
    private long mCurrentTimeMillis;

    /** Resets to default time values. */
    public void resetTimes() {
        synchronized (mLock) {
            mUptimeMillis = 10000;
            mElapsedRealtimeNanos = 20000L * TimeUtils.NANOSECONDS_PER_MILLISECOND;
            mCurrentTimeMillis = 1653000000000L; // May 19 2022 18:40:00 GMT-0400

            mThreadTimes =
                    new ThreadLocal<Long>() {
                        @Override
                        protected Long initialValue() {
                            return 0L;
                        }
                    };
        }
    }

    private final TimeUtils.FakeClock mFakeClock =
            new FakeClock() {
                @Override
                public long uptimeMillis() {
                    synchronized (mLock) {
                        return mUptimeMillis;
                    }
                }

                @Override
                public long elapsedRealtimeNanos() {
                    synchronized (mLock) {
                        return mElapsedRealtimeNanos;
                    }
                }

                @Override
                public long currentThreadTimeMillis() {
                    synchronized (mLock) {
                        return mThreadTimes.get();
                    }
                }

                @Override
                public long currentTimeMillis() {
                    synchronized (mLock) {
                        return mCurrentTimeMillis;
                    }
                }
            };

    /** Advances uptime, elapsedRealtime, and the current thread's threadTime.. */
    public void advanceMillis(long increment) {
        assert increment > 0 : "Negative increment: " + increment;
        synchronized (mLock) {
            mCurrentTimeMillis += increment;
            mUptimeMillis += increment;
            mElapsedRealtimeNanos += increment * TimeUtils.NANOSECONDS_PER_MILLISECOND;
            mThreadTimes.set(mThreadTimes.get() + increment);
        }
    }

    /** Advances uptime and elapsedRealtime. */
    public void sleepMillis(long duration) {
        assert duration > 0 : "Negative duration: " + duration;
        synchronized (mLock) {
            mCurrentTimeMillis += duration;
            mUptimeMillis += duration;
            mElapsedRealtimeNanos += duration * TimeUtils.NANOSECONDS_PER_MILLISECOND;
        }
    }

    /** Advances elapsedRealtime. */
    public void deepSleepMillis(long duration) {
        assert duration > 0 : "Negative duration: " + duration;
        synchronized (mLock) {
            mCurrentTimeMillis += duration;
            mElapsedRealtimeNanos += duration * TimeUtils.NANOSECONDS_PER_MILLISECOND;
        }
    }

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                try {
                    resetTimes();
                    TimeUtils.sFakeClock = mFakeClock;
                    base.evaluate();
                } finally {
                    TimeUtils.sFakeClock = null;
                }
            }
        };
    }
}
