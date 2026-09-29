// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
package org.chromium.base.test.util;

import android.os.Debug;

import org.chromium.build.BuildConfig;

/** Encapsulates timeout logic, and disables timeouts when debugger is attached. */
public class TimeoutTimer {
    private static final long MS_TO_NANO = 1000000;

    private final long mEndTimeNano;
    private final long mTimeoutMs;

    /**
     * @param timeoutMs Relative time for the timeout (unscaled).
     */
    public TimeoutTimer(long timeoutMs) {
        mTimeoutMs = ScalableTimeout.scaleTimeout(timeoutMs);
        mEndTimeNano = System.nanoTime() + mTimeoutMs * MS_TO_NANO;
    }

    /** Whether this timer has expired. */
    public boolean isTimedOut() {
        return getRemainingMs() == 0;
    }

    private static boolean shouldPauseTimeouts() {
        if (!BuildConfig.IS_ROBOLECTRIC) {
            return Debug.isDebuggerConnected();
        }
        // Our test runner sets this when --wait-for-java-debugger is passed.
        // This will cause tests to never time out since the value is not updated when debugger
        // detaches (oh well).
        return "true".equals(System.getProperty("chromium.jdwp_active"));
    }

    /** Returns how much time is left in milliseconds. */
    public long getRemainingMs() {
        if (shouldPauseTimeouts()) {
            // Never decreases, but still short enough that it's safe to wait() on and have a
            // timeout happen once the debugger detaches.
            return mTimeoutMs;
        }
        long ret = mEndTimeNano - System.nanoTime();
        return ret < 0 ? 0 : ret / MS_TO_NANO;
    }
}
