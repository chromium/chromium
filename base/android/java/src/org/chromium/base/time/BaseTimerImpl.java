// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;

import java.util.concurrent.TimeUnit;

/**
 * Base implementation of {@link Timer} that uses a provided time source to track elapsed real time.
 */
class BaseTimerImpl implements Timer {
    /** The granularity of the time source this timer uses. */
    private final TimeUnit mSourceTimeUnit;
    /** The provider of the current time, which should be monotonically non-decreasing. */
    protected Supplier<Long> mTimeSource;

    /**
     * Starting time of the timer. This is equal to 0 if the timer has never been started, and
     * otherwise is equal to the time at which the timer was most recently started.
     */
    private long mStartTime;
    /**
     * Stop time of the timer. This is equal to 0 if the timer is started or has never been
     * stopped, and otherwise is equal to the time at which the timer was most recently stopped.
     */
    private long mStopTime;
    /**
     * Whether the timer is currently running, i.e. if start() has been called without a
     * corresponding call to stop().
     */
    private boolean mIsRunning;

    /**
     * Constructs a new BaseTimerImpl
     * @param timeSource Provider of monotonically non-decreasing time.
     * @param timeUnit Granularity of the provided time source.
     */
    public BaseTimerImpl(@NonNull Supplier<Long> timeSource, @NonNull TimeUnit timeUnit) {
        mSourceTimeUnit = timeUnit;
        mTimeSource = timeSource;
    }

    @Override
    public void start() {
        assert !mIsRunning;
        mStopTime = 0;
        mStartTime = mTimeSource.get();
        mIsRunning = true;
    }

    @Override
    public void stop() {
        assert mIsRunning;
        mStopTime = mTimeSource.get();
        mIsRunning = false;
    }

    @Override
    public long getElapsedTime(@NonNull TimeUnit timeUnit) {
        long duration = isRunning() ? mTimeSource.get() - mStartTime : mStopTime - mStartTime;
        return timeUnit.convert(duration, mSourceTimeUnit);
    }

    @Override
    public boolean isRunning() {
        return mIsRunning;
    }
}
