// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import java.util.concurrent.TimeUnit;

/**
 * Test implementation of {@link Timer} with a directly increment-able elapsed time.
 */
public class TestTimer implements Timer {
    private final BaseTimerImpl mBaseTimer;
    private long mTime;

    public TestTimer(long startTime) {
        mTime = startTime;
        mBaseTimer = new BaseTimerImpl(this::getTime, TimeUnit.NANOSECONDS);
    }

    public void advanceBy(TimeUnit timeUnit, long increment) {
        mTime += TimeUnit.NANOSECONDS.convert(increment, timeUnit);
    }

    private long getTime() {
        return mTime;
    }

    @Override
    public void start() {
        mBaseTimer.start();
    }

    @Override
    public void stop() {
        mBaseTimer.stop();
    }

    @Override
    public long getElapsedTime(TimeUnit timeUnit) {
        return mBaseTimer.getElapsedTime(timeUnit);
    }

    @Override
    public boolean isRunning() {
        return mBaseTimer.isRunning();
    }
}
