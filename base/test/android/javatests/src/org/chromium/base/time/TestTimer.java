// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import java.util.concurrent.TimeUnit;

/**
 * Test implementation of {@link Timer} with a directly increment-able elapsed time.
 * TestTimer internally represents time as Nanoseconds to avoid rounding issues.
 */
public class TestTimer extends Timer {
    private long mTimeNano;

    public TestTimer(TimeUnit timeUnit, long startTime) {
        super(TimerType.TEST_TIME);
        mTimeNano = TimeUnit.NANOSECONDS.convert(startTime, timeUnit);
    }

    public void advanceBy(TimeUnit timeUnit, long increment) {
        mTimeNano += TimeUnit.NANOSECONDS.convert(increment, timeUnit);
    }

    @Override
    protected long getCurrentTimeNano() {
        return mTimeNano;
    }
}
