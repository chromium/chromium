// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;

/** Tests for {@link ScrollTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public final class ScrollTrackerTest {
    // The delay time used when scheduling the report task.
    private static final long DELAY_TIME_MS = 200L;
    // After this much idle time, we're guaranteed to have all scroll events reported.
    private static final long STABLE_TIME_MS = DELAY_TIME_MS * 2;
    private ScrollTrackerForTest mScrollTracker;

    @Before
    public void setUp() {
        mScrollTracker = new ScrollTrackerForTest();
    }

    @Test
    public void testSinglePositiveScroll_scrollWrongDirection() {
        mScrollTracker.trackScroll(10, 0);
        advanceByMs(STABLE_TIME_MS);

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList());
    }

    @Test
    public void testNoScroll() {
        mScrollTracker.trackScroll(0, 0);
        advanceByMs(STABLE_TIME_MS);

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList());
    }

    @Test
    public void testSinglePositiveScroll() {
        mScrollTracker.trackScroll(0, 10);
        advanceByMs(DELAY_TIME_MS);

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(10));
    }

    @Test
    public void testUnbindReportsImmediately() {
        mScrollTracker.trackScroll(0, 10);
        mScrollTracker.onUnbind();

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(10));
    }

    @Test
    public void testScrollAfterUnbindWorks() {
        mScrollTracker.trackScroll(0, 10);
        mScrollTracker.onUnbind();
        mScrollTracker.trackScroll(0, 5);
        advanceByMs(STABLE_TIME_MS);

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(10, 5));
    }

    @Test
    public void testUnbindWithoutScroll() {
        mScrollTracker.onUnbind();

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList());
    }

    @Test
    public void testDoublePositiveScroll_singleTask() {
        mScrollTracker.trackScroll(0, 10);
        advanceByMs(DELAY_TIME_MS - 1);
        mScrollTracker.trackScroll(0, 5);
        advanceByMs(STABLE_TIME_MS);

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(15));
    }

    @Test
    public void testDoublePositiveScroll_doubleTask() {
        mScrollTracker.trackScroll(0, 10);
        advanceByMs(STABLE_TIME_MS);
        mScrollTracker.trackScroll(0, 5);
        advanceByMs(STABLE_TIME_MS);

        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(10, 5));
    }

    @Test
    public void testSwitchScrollDirection() {
        mScrollTracker.trackScroll(0, 10);
        advanceByMs(1L);
        mScrollTracker.trackScroll(0, -5);
        // Reports 10 immediately.
        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(10));

        // Eventually, -5 is reported as well.
        advanceByMs(STABLE_TIME_MS);
        assertEquals(mScrollTracker.scrollAmounts, Arrays.asList(10, -5));
    }

    static void advanceByMs(long timeMs) {
        Robolectric.getForegroundThreadScheduler().advanceBy(timeMs, TimeUnit.MILLISECONDS);
    }

    static class ScrollTrackerForTest extends ScrollTracker {
        public ArrayList<Integer> scrollAmounts = new ArrayList<>();

        ScrollTrackerForTest() {
            super();
        }

        @Override
        protected void onScrollEvent(int scrollAmount) {
            scrollAmounts.add(scrollAmount);
        }
    }
}
