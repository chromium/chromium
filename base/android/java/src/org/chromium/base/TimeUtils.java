// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/** Time-related utilities. */
@JNINamespace("base::android")
@MainDex
public class TimeUtils {
    private TimeUtils() {}

    // For conversion from MILLISECONDS use {@android.text.format.DateUtils#*_IN_MILLIS}
    public static final int NANOSECONDS_PER_MILLISECOND = 1000000;
    public static final int SECONDS_PER_MINUTE = 60;
    public static final int SECONDS_PER_HOUR = 3600; // 60 sec * 60 min
    public static final int SECONDS_PER_DAY = 86400;

    @NativeMethods
    public interface Natives {
        // 60 sec * 60 min * 24 h

        /** Returns TimeTicks::Now() in microseconds. */
        long getTimeTicksNowUs();
    }
}
