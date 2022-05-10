// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import androidx.annotation.NonNull;

import java.util.concurrent.TimeUnit;

/**
 * Interface for a provider of elapsed time, useful for general purpose interval timing.
 * Implementations should guarantee that returned values are monotonically non-decreasing.
 */
public interface Timer {
    /** Starts the timer. This resets the stop time to 0 if it had been previously set. */
    void start();

    /** Stops the timer. */
    void stop();

    /** Returns whether the timer is currently running. */
    boolean isRunning();

    /**
     * Returns the elapsed time. This is either the delta between start and stop, or the difference
     * between start and when this method is called (if the timer is still running). The result is
     * in terms of the desired TimeUnit, rounding down.
     */
    long getElapsedTime(@NonNull TimeUnit timeUnit);
}
