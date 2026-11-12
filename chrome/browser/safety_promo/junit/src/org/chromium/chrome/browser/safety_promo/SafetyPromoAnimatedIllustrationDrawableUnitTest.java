// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.GraphicsMode;
import org.robolectric.annotation.GraphicsMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for {@link SafetyPromoAnimatedIllustrationDrawable}. */
@RunWith(BaseRobolectricTestRunner.class)
@GraphicsMode(Mode.NATIVE)
public class SafetyPromoAnimatedIllustrationDrawableUnitTest {
    private static final int TOP_LEFT = 0;
    private static final int TOP_RIGHT = 3;
    private static final int BOTTOM_LEFT = 8;
    private static final int BOTTOM_RIGHT = 11;

    private static final long STEP_MS = SafetyPromoAnimatedIllustrationDrawable.STEP_DURATION_MS;
    private static final long FADE_MS = SafetyPromoAnimatedIllustrationDrawable.FADE_DURATION_MS;
    private static final int BRIGHT_ALPHA = 255;
    private static final int DIM_ALPHA =
            Math.round(SafetyPromoAnimatedIllustrationDrawable.DIM_ICON_ALPHA * 255);

    private static final int SIMULATED_STEPS = 120;
    private static final int TEST_STEP = 5;
    private static final long FRAME_INTERVAL_MS = 16;

    private Context mContext;
    private SafetyPromoAnimatedIllustrationDrawable mDrawable;
    private int mWidthPx;
    private int mHeightPx;
    private int mIconSizePx;
    private int mLogoSizePx;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        Resources res = mContext.getResources();
        mWidthPx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_width);
        mHeightPx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_height);
        mIconSizePx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_icon_size);
        mLogoSizePx = res.getDimensionPixelSize(R.dimen.safety_fre_animated_illustration_logo_size);
        mDrawable = SafetyPromoAnimatedIllustrationDrawable.create(mContext);
    }

    @After
    public void tearDown() {
        mDrawable.stop();
    }

    @Test
    public void testIntrinsicSize_matchesDimens() {
        assertEquals(mWidthPx, mDrawable.getIntrinsicWidth());
        assertEquals(mHeightPx, mDrawable.getIntrinsicHeight());
    }

    @Test
    public void testOnBoundsChange_centersLayoutInBounds() {
        mDrawable.setBounds(10, 20, 10 + mWidthPx + 40, 20 + mHeightPx + 40);
        int left = 30;
        int top = 40;
        int right = left + mWidthPx;
        int bottom = top + mHeightPx;

        int logoLeft = left + (mWidthPx - mLogoSizePx) / 2;
        int logoTop = top + (mHeightPx - mLogoSizePx) / 2;
        assertEquals(
                new Rect(logoLeft, logoTop, logoLeft + mLogoSizePx, logoTop + mLogoSizePx),
                mDrawable.getLogoForTesting().getBounds());

        List<Drawable> icons = mDrawable.getIconsForTesting();
        assertEquals(
                new Rect(left, top, left + mIconSizePx, top + mIconSizePx),
                icons.get(TOP_LEFT).getBounds());
        assertEquals(
                new Rect(right - mIconSizePx, top, right, top + mIconSizePx),
                icons.get(TOP_RIGHT).getBounds());
        assertEquals(
                new Rect(right - mIconSizePx, bottom - mIconSizePx, right, bottom),
                icons.get(BOTTOM_RIGHT).getBounds());
        assertEquals(
                new Rect(left, bottom - mIconSizePx, left + mIconSizePx, bottom),
                icons.get(BOTTOM_LEFT).getBounds());
    }

    @Test
    public void testOnTimeUpdate_neverExceedsMaxDimIconsAtEndOfStep() {
        int maxDimIcons = 0;
        for (int step = 0; step < SIMULATED_STEPS; step++) {
            mDrawable.onTimeUpdate(getEndOfStep(step));
            int dimIcons = countDimIcons(mDrawable);
            assertTrue(
                    "Too many dim icons at step " + step,
                    dimIcons <= SafetyPromoAnimatedIllustrationDrawable.MAX_DIM_ICONS);
            maxDimIcons = Math.max(maxDimIcons, dimIcons);
        }
        assertTrue("No icon was ever dim", maxDimIcons > 0);
    }

    @Test
    public void testOnTimeUpdate_fadesFromPreviousStateThenHolds() {
        boolean sawFade = false;
        for (int step = 1; step < SIMULATED_STEPS; step++) {
            mDrawable.onTimeUpdate(getEndOfStep(step - 1));
            int[] from = getIconAlphas(mDrawable);
            mDrawable.onTimeUpdate(step * STEP_MS + FADE_MS / 2);
            int[] during = getIconAlphas(mDrawable);
            mDrawable.onTimeUpdate(step * STEP_MS + FADE_MS);
            int[] to = getIconAlphas(mDrawable);
            mDrawable.onTimeUpdate(getEndOfStep(step));
            assertArrayEquals("Alphas changed while holding", to, getIconAlphas(mDrawable));
            for (int i = 0; i < from.length; i++) {
                assertTrue(
                        "Icon " + i + " did not settle at step " + step,
                        to[i] == BRIGHT_ALPHA || to[i] == DIM_ALPHA);
                if (from[i] == to[i]) continue;

                sawFade = true;
                assertTrue(
                        "Icon " + i + " did not fade at step " + step,
                        during[i] > Math.min(from[i], to[i])
                                && during[i] < Math.max(from[i], to[i]));
            }
        }
        assertTrue("No icon ever faded", sawFade);
    }

    @Test
    public void testOnTimeUpdate_doesNotInvalidateWhileHolding() {
        int[] invalidations = new int[1];
        mDrawable.setCallback(
                new Drawable.Callback() {
                    @Override
                    public void invalidateDrawable(Drawable who) {
                        invalidations[0]++;
                    }

                    @Override
                    public void scheduleDrawable(Drawable who, Runnable what, long when) {}

                    @Override
                    public void unscheduleDrawable(Drawable who, Runnable what) {}
                });
        mDrawable.onTimeUpdate(TEST_STEP * STEP_MS + FADE_MS);
        invalidations[0] = 0;

        mDrawable.onTimeUpdate(TEST_STEP * STEP_MS + FADE_MS + (STEP_MS - FADE_MS) / 2);
        mDrawable.onTimeUpdate(getEndOfStep(TEST_STEP));

        assertEquals(0, invalidations[0]);
    }

    @Test
    public void testBlinkingDoesNotDependOnFrameRate() {
        SafetyPromoAnimatedIllustrationDrawable otherDrawable =
                SafetyPromoAnimatedIllustrationDrawable.create(mContext);

        // The other drawable only gets a frame every three steps, so it has to catch up on several
        // steps at once.
        long timeMs = 0;
        for (int step = 2; step < 30; step += 3) {
            long sampleMs = getEndOfStep(step);
            for (; timeMs < sampleMs; timeMs += FRAME_INTERVAL_MS) {
                mDrawable.onTimeUpdate(timeMs);
            }
            mDrawable.onTimeUpdate(sampleMs);
            otherDrawable.onTimeUpdate(sampleMs);
            assertArrayEquals(
                    "Alphas differ at step " + step,
                    getIconAlphas(mDrawable),
                    getIconAlphas(otherDrawable));
        }
    }

    @Test
    public void testStop_resetsIconsAndRestartsFromBeginning() {
        mDrawable.start();
        mDrawable.onTimeUpdate(getEndOfStep(TEST_STEP));
        int[] alphas = getIconAlphas(mDrawable);
        assertTrue("Some icons should be dim", countDimIcons(mDrawable) > 0);

        mDrawable.stop();
        assertFalse(mDrawable.isRunning());
        assertAllIconsBright(mDrawable);

        mDrawable.start();
        mDrawable.onTimeUpdate(getEndOfStep(TEST_STEP));
        assertArrayEquals(alphas, getIconAlphas(mDrawable));
    }

    @Test
    public void testSetVisible_startsRestartsAndStopsAnimation() {
        mDrawable.setVisible(/* visible= */ false, /* restart= */ true);
        mDrawable.setVisible(/* visible= */ true, /* restart= */ false);
        assertTrue(mDrawable.isRunning());

        mDrawable.onTimeUpdate(getEndOfStep(TEST_STEP));
        assertTrue("Some icons should be dim", countDimIcons(mDrawable) > 0);
        mDrawable.setVisible(/* visible= */ true, /* restart= */ true);
        assertTrue(mDrawable.isRunning());
        assertAllIconsBright(mDrawable);

        mDrawable.onTimeUpdate(getEndOfStep(TEST_STEP));
        assertTrue("Some icons should be dim", countDimIcons(mDrawable) > 0);
        mDrawable.setVisible(/* visible= */ false, /* restart= */ false);
        assertFalse(mDrawable.isRunning());
        assertAllIconsBright(mDrawable);
    }

    @Test
    public void testStart_animatorsDisabled_doesNotRun() {
        SafetyPromoAnimatedIllustrationDrawable.setAreAnimatorsEnabledForTesting(
                /* enabled= */ false);

        mDrawable.start();

        assertFalse(mDrawable.isRunning());
    }

    /** Returns the time just before the end of {@code step}, when fading icons have finished. */
    private static long getEndOfStep(int step) {
        return (step + 1) * STEP_MS - 1;
    }

    private static int[] getIconAlphas(SafetyPromoAnimatedIllustrationDrawable drawable) {
        List<Drawable> icons = drawable.getIconsForTesting();
        int[] alphas = new int[icons.size()];
        for (int i = 0; i < icons.size(); i++) {
            alphas[i] = icons.get(i).getAlpha();
        }
        return alphas;
    }

    private static int countDimIcons(SafetyPromoAnimatedIllustrationDrawable drawable) {
        int count = 0;
        for (int alpha : getIconAlphas(drawable)) {
            if (isDim(alpha)) count++;
        }
        return count;
    }

    private static boolean isDim(int alpha) {
        return alpha < BRIGHT_ALPHA;
    }

    private static void assertAllIconsBright(SafetyPromoAnimatedIllustrationDrawable drawable) {
        for (int alpha : getIconAlphas(drawable)) {
            assertEquals(BRIGHT_ALPHA, alpha);
        }
    }
}
