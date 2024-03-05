// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.TimeUtils.CurrentThreadTimeMillisTimer;
import org.chromium.base.TimeUtils.ElapsedRealtimeMillisTimer;
import org.chromium.base.TimeUtils.ElapsedRealtimeNanosTimer;
import org.chromium.base.TimeUtils.UptimeMillisTimer;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TimeUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimeUtilsTest {
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    @Test
    @SmallTest
    public void testTimers() {
        UptimeMillisTimer uptimeTimer = new UptimeMillisTimer();
        ElapsedRealtimeMillisTimer realtimeTimer = new ElapsedRealtimeMillisTimer();
        ElapsedRealtimeNanosTimer realtimeTimerNanos = new ElapsedRealtimeNanosTimer();
        CurrentThreadTimeMillisTimer threadTimeTimer = new CurrentThreadTimeMillisTimer();
        mFakeTime.advanceMillis(1000);
        assertEquals(1000, uptimeTimer.getElapsedMillis());
        assertEquals(1000, realtimeTimer.getElapsedMillis());
        assertEquals(
                1000 * TimeUtils.NANOSECONDS_PER_MILLISECOND, realtimeTimerNanos.getElapsedNanos());
        assertEquals(1000, threadTimeTimer.getElapsedMillis());
        mFakeTime.deepSleepMillis(1000);
        assertEquals(1000, uptimeTimer.getElapsedMillis());
        assertEquals(2000, realtimeTimer.getElapsedMillis());
        assertEquals(
                2000 * TimeUtils.NANOSECONDS_PER_MILLISECOND, realtimeTimerNanos.getElapsedNanos());
        assertEquals(1000, threadTimeTimer.getElapsedMillis());
        mFakeTime.sleepMillis(1000);
        assertEquals(2000, uptimeTimer.getElapsedMillis());
        assertEquals(3000, realtimeTimer.getElapsedMillis());
        assertEquals(
                3000 * TimeUtils.NANOSECONDS_PER_MILLISECOND, realtimeTimerNanos.getElapsedNanos());
        assertEquals(1000, threadTimeTimer.getElapsedMillis());
    }
}
