// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View.MeasureSpec;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.widget.chips.ChipView;

/**
 * Tests for the {@link KeyboardAccessoryChipGroup}. The tests intentionally use a device config
 * that maps 1dp to exactly 1 pixel. The screen resolution is 360x640.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "w360dp-h640dp-mdpi")
public class KeyboardAccessoryChipGroupTest {
    private static final int SCREEN_WIDTH = 360;
    // Use fixed height because the tests don't verify it.
    private static final int KEYBOARD_ACCESSORY_HEIGHT = 64;

    private KeyboardAccessoryChipGroup mChipGroup;

    private static class ChipViewWithFixedDimensions extends ChipView {
        private int mWidthDp;

        ChipViewWithFixedDimensions(Context context, int widthDp) {
            super(
                    context,
                    /* attrs= */ null,
                    /* defStyleAttr= */ 0,
                    R.style.KeyboardAccessoryTwoLineChip);
            mWidthDp = widthDp;
        }

        void setWidth(int widthDp) {
            mWidthDp = widthDp;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMeasuredDimension(mWidthDp, KEYBOARD_ACCESSORY_HEIGHT);
        }
    }

    @Before
    public void setUp() {
        // Set the context theme so that ChipView attributes and macros can be resolved.
        getContext().getTheme().applyStyle(R.style.Theme_BrowserUI_DayNight, true);
        // Make sure that 1dp is mapped to 1 pixel on the screen by converting 100dp to pixels.
        DisplayMetrics metrics = RuntimeEnvironment.application.getResources().getDisplayMetrics();
        assertEquals(
                100, (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 100, metrics));

        mChipGroup = new KeyboardAccessoryChipGroup(getContext());
    }

    @Test
    public void testOneSmallChip() {
        // The maximum width for single chips should not be set.
        ChipView chip = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip);
        measureChipGroup();

        assertEquals(Integer.MAX_VALUE, chip.getMaxWidth());
    }

    @Test
    public void testOneBigChip() {
        // The maximum width for single chips should not be set.
        ChipView chip = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 500);
        mChipGroup.addView(chip);
        measureChipGroup();

        assertEquals(Integer.MAX_VALUE, chip.getMaxWidth());
    }

    @Test
    public void testTwoSmallChips() {
        // The first chip doesn't exceed the screen width (360dp) so its width shouldn't be limited.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        measureChipGroup();

        assertEquals(Integer.MAX_VALUE, chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
    }

    @Test
    public void testOneBigAndOneSmallChip() {
        // The first chip exceeds the screen width (360dp) so its width should be limited so that
        // the second chip is displayed on the screen as well.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 400);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        measureChipGroup();

        assertEquals(getSingleChipBufferSize(), chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
    }

    @Test
    public void testOneSmallAndOneBigChip() {
        // The first chip doesn't exceed the screen width (360dp) so its width shouldn't be limited
        // because the second chip will be visible on the screen anyway.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 400);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        measureChipGroup();

        assertEquals(Integer.MAX_VALUE, chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
    }

    @Test
    public void testThreeSmallChips() {
        // First 2 chips don't exceed the screen width, so chip width shouldn't be limited.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        ChipView chip3 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        mChipGroup.addView(chip3);
        measureChipGroup();

        assertEquals(Integer.MAX_VALUE, chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip3.getMaxWidth());
    }

    @Test
    public void testThreeChipsFirstBigChip() {
        // The first chip exceeds the screen width, so its width should be limited so that the
        // second chip is visible on the screen.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 500);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        ChipView chip3 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        mChipGroup.addView(chip3);
        measureChipGroup();

        assertEquals(getSingleChipBufferSize(), chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip3.getMaxWidth());
    }

    @Test
    public void testThreeChipsSecondBigChip() {
        // The first chip doesn't exceed the screen width, but the first 2 chips exceed the screen
        // width, so their width should be limited so that the third ship can be shown on the
        // screen.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 120);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 240);
        ChipView chip3 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        mChipGroup.addView(chip3);
        measureChipGroup();

        // The chip width should be reduced proportionally.
        final int firstChipWidth = getTwoChipsCombinedSize() / 3;
        assertEquals(firstChipWidth, chip1.getMaxWidth());
        final int secondChipWidth = (getTwoChipsCombinedSize() * 2) / 3;
        assertEquals(secondChipWidth, chip2.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip3.getMaxWidth());
    }

    @Test
    public void testChipWidthReductionRatio() {
        // The first chip doesn't exceed the screen width, but the first 2 chips exceed the screen
        // width. If the chip group tries to fit them into the chip buffer, both of them will loose
        // a lot of information. Make sure their width is not limited in this case.
        ChipView chip1 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 500);
        ChipView chip3 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        mChipGroup.addView(chip3);
        measureChipGroup();

        // The chip width should not be reduced.
        assertEquals(Integer.MAX_VALUE, chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip3.getMaxWidth());
    }

    @Test
    public void testResetsMaxWidth() {
        // Step 1: the first chip width exceeds the screen width, so its width should be limited so
        // that the second chip can be displayed on the screen.
        ChipViewWithFixedDimensions chip1 =
                new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 500);
        ChipView chip2 = new ChipViewWithFixedDimensions(getContext(), /* widthDp= */ 100);
        mChipGroup.addView(chip1);
        mChipGroup.addView(chip2);
        measureChipGroup();

        assertEquals(getSingleChipBufferSize(), chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());

        // Step 2: reduce the first chip width so that it fits on the screen. The maximum width
        // should be reset in this case.
        chip1.setWidth(100);
        measureChipGroup();

        measureChipGroup();
        assertEquals(Integer.MAX_VALUE, chip1.getMaxWidth());
        assertEquals(Integer.MAX_VALUE, chip2.getMaxWidth());
    }

    private void measureChipGroup() {
        mChipGroup.measure(
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(KEYBOARD_ACCESSORY_HEIGHT, MeasureSpec.EXACTLY));
    }

    /**
     * Returns the preferred width of the last chip on the screen.
     *
     * @return the preferred width of the last chip on the screen.
     */
    private @Px int getLastChipPeekWidth() {
        return getContext()
                .getResources()
                .getDimensionPixelSize(R.dimen.keyboard_accessory_last_chip_peek_width);
    }

    /**
     * Return the margin between the Keyboard accessory chips. This margin applies to the start and
     * the end of the suggestion list in the keyboard accessory.
     *
     * @return the margin between the Keyboard accessory chips.
     */
    private @Px int getChipMargin() {
        return getContext()
                .getResources()
                .getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding);
    }

    /**
     * Returns the preferred size of a single chip that doesn't fit into the screen width initially.
     *
     * @return the preferred size of a single chip.
     */
    private @Px int getSingleChipBufferSize() {
        return SCREEN_WIDTH - 2 * getChipMargin() - getLastChipPeekWidth();
    }

    /**
     * Returns the combined preferred size of 2 chips that do not fit into the screen width
     * initially.
     *
     * @return the combined preferred size of 2 chips.
     */
    private @Px int getTwoChipsCombinedSize() {
        // Just subtract a single margin between these 2 chips.
        return getSingleChipBufferSize() - getChipMargin();
    }

    private Context getContext() {
        return RuntimeEnvironment.application;
    }
}
