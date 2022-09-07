// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.base.test.util;

import android.os.Debug;
import android.os.SystemClock;

/**
 * Encapsulates timeout logic, and disables timeouts when debugger is attached.
 */
public class TimeoutTimer {
    private final long mEndTimeMs;
    private final long mTimeoutMs;

    /**
     * @param timeoutMs Relative time for the timeout (unscaled).
     */
    public TimeoutTimer(long timeoutMs) {
        mTimeoutMs = ScalableTimeout.scaleTimeout(timeoutMs);
        mEndTimeMs = SystemClock.uptimeMillis() + mTimeoutMs;
    }

    /** Whether this timer has expired. */
    public boolean isTimedOut() {
        return getRemainingMs() == 0;
    }

    /** Returns how much time is left in milliseconds. */
    public long getRemainingMs() {
        if (Debug.isDebuggerConnected()) {
            // Never decreases, but still short enough that it's safe to wait() on and have a
            // timeout happen once the debugger detaches.
            return mTimeoutMs;
        }
        long ret = mEndTimeMs - SystemClock.uptimeMillis();
        return ret < 0 ? 0 : ret;
    }
}
