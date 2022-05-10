// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import android.os.SystemClock;

import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link Timer} that uses the Android's SystemClock to track elapsed real time at
 * an internal resolution of nanoseconds. This is a good default choice for interval timing.
 */
public class ElapsedRealTimeTimer extends BaseTimerImpl {
    public ElapsedRealTimeTimer() {
        super(SystemClock::elapsedRealtimeNanos, TimeUnit.NANOSECONDS);
    }
}
