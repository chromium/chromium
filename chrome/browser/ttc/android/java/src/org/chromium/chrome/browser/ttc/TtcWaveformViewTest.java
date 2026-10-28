// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ttc.TtcWaveformView.Mode;

/** Unit tests for {@link TtcWaveformView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TtcWaveformViewTest {
    private static final int CENTER = TtcWaveformView.BAR_COUNT / 2;

    private TtcWaveformView mView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new TtcWaveformView(activity);
    }

    @Test
    public void testStartsIdleWithMinimumBars() {
        assertEquals(Mode.IDLE, mView.getModeForTesting());
        assertAllBarsAtMinimum();
    }

    @Test
    public void testBarHeightMapping() {
        assertEquals(TtcWaveformView.MIN_BAR_HEIGHT_DP, TtcWaveformView.calculateBarHeight(0f), 0f);
        assertEquals(
                TtcWaveformView.MIN_BAR_HEIGHT_DP, TtcWaveformView.calculateBarHeight(-1f), 0f);
        assertEquals(
                TtcWaveformView.MAX_BAR_HEIGHT_DP, TtcWaveformView.calculateBarHeight(1f), 0.001f);

        // Louder input never makes a bar shorter.
        float previous = TtcWaveformView.calculateBarHeight(0f);
        for (float level = 0.01f; level <= 1f; level += 0.01f) {
            float height = TtcWaveformView.calculateBarHeight(level);
            assertTrue("Height decreased at level " + level, height >= previous);
            previous = height;
        }
    }

    @Test
    public void testActiveBarsFollowAudioFromTheCenter() {
        mView.setMode(Mode.ACTIVE);
        mView.setAudioLevel(1f);
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);

        float[] heights = mView.getBarHeightsForTesting();
        assertTrue(heights[CENTER] > TtcWaveformView.MIN_BAR_HEIGHT_DP);
        // The level reaches the outer bars only after it has propagated out.
        assertEquals(TtcWaveformView.MIN_BAR_HEIGHT_DP, heights[1], 0f);

        for (int i = 0; i < TtcWaveformView.BAR_COUNT; i++) {
            mView.setAudioLevel(1f);
            mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        }
        heights = mView.getBarHeightsForTesting();
        assertTrue(heights[1] > TtcWaveformView.MIN_BAR_HEIGHT_DP);
        // Bars are symmetric around the center.
        for (int i = 0; i < CENTER; i++) {
            assertEquals(heights[i], heights[TtcWaveformView.BAR_COUNT - 1 - i], 0f);
        }
        // Bars taper towards the edges.
        assertTrue(heights[CENTER] > heights[1]);
        assertTrue(heights[1] >= heights[0]);
        for (float height : heights) {
            assertTrue(height <= TtcWaveformView.MAX_BAR_HEIGHT_DP);
        }
    }

    @Test
    public void testActiveBarsSettleWhenAudioStops() {
        mView.setMode(Mode.ACTIVE);
        mView.setAudioLevel(1f);
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);

        // No more samples: the last level is held for one step, then decays to silence.
        for (int i = 0; i < 100; i++) {
            mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        }
        assertAllBarsAtMinimum();
    }

    @Test
    public void testHoldsLastLevelForOneStep() {
        mView.setMode(Mode.ACTIVE);
        mView.setAudioLevel(1f);
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        float afterSample = mView.getBarHeightsForTesting()[CENTER];

        // The next sample is late: the center bar keeps rising towards the held level.
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        float afterHeldStep = mView.getBarHeightsForTesting()[CENTER];
        assertTrue(afterHeldStep > afterSample);

        // Still no sample: the center bar falls.
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        assertTrue(mView.getBarHeightsForTesting()[CENTER] < afterHeldStep);
    }

    @Test
    public void testAtRestUntilAudioRaisesTheBars() {
        mView.setMode(Mode.ACTIVE);
        assertTrue(mView.isAtRestForTesting());

        // Silence, and invalid levels, don't wake up the animation.
        mView.setAudioLevel(0f);
        mView.setAudioLevel(Float.NaN);
        assertTrue(mView.isAtRestForTesting());

        mView.setAudioLevel(1f);
        assertFalse(mView.isAtRestForTesting());
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        assertFalse(mView.isAtRestForTesting());

        // Once the level has propagated out and the bars have settled, the animation can stop.
        for (int i = 0; i < 100; i++) {
            mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        }
        assertTrue(mView.isAtRestForTesting());
        assertAllBarsAtMinimum();
    }

    @Test
    public void testIgnoresAudioLevelsWhenIdle() {
        mView.setAudioLevel(1f);
        mView.setMode(Mode.ACTIVE);
        assertTrue(mView.isAtRestForTesting());

        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        assertAllBarsAtMinimum();
    }

    @Test
    public void testEasingDoesNotDependOnFrameRate() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        TtcWaveformView otherView = new TtcWaveformView(activity);
        for (TtcWaveformView view : new TtcWaveformView[] {mView, otherView}) {
            view.setMode(Mode.ACTIVE);
            view.setAudioLevel(1f);
            view.updateBars(TtcWaveformView.HISTORY_STEP_MS);
        }

        // Two 120 Hz frames move the bars as far as one 60 Hz frame.
        mView.updateBars(8);
        mView.updateBars(8);
        otherView.updateBars(16);
        float[] heights = mView.getBarHeightsForTesting();
        float[] otherHeights = otherView.getBarHeightsForTesting();
        for (int i = 0; i < TtcWaveformView.BAR_COUNT; i++) {
            assertEquals(otherHeights[i], heights[i], 0.001f);
        }
        assertTrue(heights[CENTER] < TtcWaveformView.MAX_BAR_HEIGHT_DP);
    }

    @Test
    public void testLeavingActiveResetsBars() {
        mView.setMode(Mode.ACTIVE);
        mView.setAudioLevel(1f);
        mView.updateBars(TtcWaveformView.HISTORY_STEP_MS);

        mView.setMode(Mode.IDLE);
        assertEquals(Mode.IDLE, mView.getModeForTesting());
        assertAllBarsAtMinimum();
        assertTrue(mView.isAtRestForTesting());
    }

    private void assertAllBarsAtMinimum() {
        for (float height : mView.getBarHeightsForTesting()) {
            assertEquals(TtcWaveformView.MIN_BAR_HEIGHT_DP, height, 0.001f);
        }
    }
}
