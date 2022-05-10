// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import android.os.SystemClock;

import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link Timer} that uses the Android SystemClock's uptimeMillis to track elapsed
 * time excluding deep sleep.
 */
public class UptimeTimer extends BaseTimerImpl {
    public UptimeTimer() {
        super(SystemClock::uptimeMillis, TimeUnit.MILLISECONDS);
    }
}
