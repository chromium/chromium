// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TtcSessionWindowTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TtcSessionWindowTrackerTest {
    private final Object mWindowA = new Object();
    private final Object mWindowB = new Object();

    private int mEndSessionCount;
    private TtcSessionWindowTracker mTracker;

    @Before
    public void setUp() {
        mTracker = new TtcSessionWindowTracker(() -> mEndSessionCount++);
    }

    @Test
    public void testEndsSessionOnlyWhenLastWindowRemoved() {
        mTracker.addVisibleWindow(mWindowA);
        mTracker.addVisibleWindow(mWindowB);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testRemovingUntrackedWindowIsNoOp() {
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        mTracker.addVisibleWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(0, mEndSessionCount);
    }

    @Test
    public void testRemovingWindowTwiceEndsSessionOnce() {
        mTracker.addVisibleWindow(mWindowA);

        mTracker.removeVisibleWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testAddingWindowTwiceIsIdempotent() {
        mTracker.addVisibleWindow(mWindowA);
        mTracker.addVisibleWindow(mWindowA);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testTracksWindowsAgainAfterSessionEnded() {
        mTracker.addVisibleWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);

        mTracker.addVisibleWindow(mWindowA);
        mTracker.addVisibleWindow(mWindowB);
        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(1, mEndSessionCount);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(2, mEndSessionCount);
    }
}
