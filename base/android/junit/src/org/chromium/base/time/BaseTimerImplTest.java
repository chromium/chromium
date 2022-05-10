// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.time;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link BaseTimerImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseTimerImplTest {
    private long mCurrentTime;
    private BaseTimerImpl mBaseTimer;

    @Before
    public void setUp() {
        mBaseTimer = new BaseTimerImpl(() -> mCurrentTime, TimeUnit.NANOSECONDS);
    }

    @Test
    @SmallTest
    public void testStartStopRunning() {
        assertFalse(mBaseTimer.isRunning());

        for (int i = 0; i < 5; i++) {
            mBaseTimer.start();
            assertTrue(mBaseTimer.isRunning());

            mBaseTimer.stop();
            assertFalse(mBaseTimer.isRunning());
        }
    }

    @Test
    @SmallTest
    public void testGetElapsedTime() {
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.NANOSECONDS), 0L);
        mBaseTimer.start();
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.NANOSECONDS), 0L);

        mCurrentTime = 1000L;
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.NANOSECONDS), 1000L);
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.MICROSECONDS), 1L);

        mBaseTimer.stop();
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.NANOSECONDS), 1000L);
        mCurrentTime = 2000L;
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.NANOSECONDS), 1000L);
    }

    @Test
    @SmallTest
    public void testRounding() {
        mBaseTimer.start();
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.NANOSECONDS), 0L);

        mCurrentTime = 1500L;
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.MICROSECONDS), 1L);
        mCurrentTime = 1501L;
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.MICROSECONDS), 1L);
        mCurrentTime = 1999L;
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.MICROSECONDS), 1L);
        mCurrentTime = 2000L;
        assertEquals(mBaseTimer.getElapsedTime(TimeUnit.MICROSECONDS), 2L);
    }

    @Test
    @SmallTest
    public void testLowerGranularitySource() {
        BaseTimerImpl lowerGranularityTimer =
                new BaseTimerImpl(() -> mCurrentTime, TimeUnit.MILLISECONDS);
        lowerGranularityTimer.start();

        mCurrentTime = 1001L;
        assertEquals(lowerGranularityTimer.getElapsedTime(TimeUnit.SECONDS), 1L);
        assertEquals(lowerGranularityTimer.getElapsedTime(TimeUnit.MILLISECONDS), 1001L);
        assertEquals(lowerGranularityTimer.getElapsedTime(TimeUnit.MICROSECONDS), 1001000L);
        assertEquals(lowerGranularityTimer.getElapsedTime(TimeUnit.NANOSECONDS), 1001000000L);
    }
}