// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import static junit.framework.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link Timer} that uses {@link TestTimer} to carefully control time changes.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TimerTest {
    private long mCurrentTime;
    private TestTimer mTimer;

    @Before
    public void setUp() {
        mTimer = new TestTimer(TimeUnit.NANOSECONDS, 0);
    }

    @Test
    @SmallTest
    public void testRestart() {
        for (int i = 0; i < 5; i++) {
            mTimer.advanceBy(TimeUnit.NANOSECONDS, 100);
            assertEquals(100L, mTimer.getElapsedTime(TimeUnit.NANOSECONDS));
            mTimer.restart();
        }
    }

    @Test
    @SmallTest
    public void testGetElapsedTime() {
        assertEquals(0L, mTimer.getElapsedTime(TimeUnit.NANOSECONDS));
        assertEquals(0L, mTimer.getElapsedTime(TimeUnit.NANOSECONDS));

        mTimer.advanceBy(TimeUnit.NANOSECONDS, 1000L);
        assertEquals(1000L, mTimer.getElapsedTime(TimeUnit.NANOSECONDS));
        assertEquals(1L, mTimer.getElapsedTime(TimeUnit.MICROSECONDS));
    }

    @Test
    @SmallTest
    public void testRounding() {
        assertEquals(0L, mTimer.getElapsedTime(TimeUnit.NANOSECONDS));

        mTimer.advanceBy(TimeUnit.NANOSECONDS, 1500L);
        assertEquals(1L, mTimer.getElapsedTime(TimeUnit.MICROSECONDS));
        mTimer.advanceBy(TimeUnit.NANOSECONDS, 1L);
        assertEquals(1L, mTimer.getElapsedTime(TimeUnit.MICROSECONDS));
        mTimer.advanceBy(TimeUnit.NANOSECONDS, 498L);
        assertEquals(1L, mTimer.getElapsedTime(TimeUnit.MICROSECONDS));
        mTimer.advanceBy(TimeUnit.NANOSECONDS, 1L);
        assertEquals(2L, mTimer.getElapsedTime(TimeUnit.MICROSECONDS));
    }

    @Test
    @SmallTest
    public void testLowerGranularitySource() {
        mTimer.advanceBy(TimeUnit.MILLISECONDS, 1001L);
        assertEquals(1001000000L, mTimer.getElapsedTime(TimeUnit.NANOSECONDS));
        assertEquals(1001000L, mTimer.getElapsedTime(TimeUnit.MICROSECONDS));
        assertEquals(1001L, mTimer.getElapsedTime(TimeUnit.MILLISECONDS));
        assertEquals(1L, mTimer.getElapsedTime(TimeUnit.SECONDS));
    }
}
