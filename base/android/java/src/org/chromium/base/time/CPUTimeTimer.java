// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import android.os.SystemClock;

import java.util.concurrent.TimeUnit;

/**
 * Implementation of {@link Timer} that uses the Android SystemClock's currentThreadTimeMillis to
 * track CPU thread time. This is typically *not* what you want for interval timing, and should only
 * be used if you're sure what you're measuring is CPU bound.
 */
public class CPUTimeTimer extends BaseTimerImpl {
    public CPUTimeTimer() {
        super(SystemClock::currentThreadTimeMillis, TimeUnit.MILLISECONDS);
    }
}
