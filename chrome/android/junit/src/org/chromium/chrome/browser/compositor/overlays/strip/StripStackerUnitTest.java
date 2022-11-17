// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link StripStacker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripStackerUnitTest {
    private static final float TAB_WIDTH = 25;
    private static final float CACHED_TAB_WIDTH = 30;
    private static final float TAB_WEIGHT = 1;
    private static final float TAB_OVERLAP = 5;
    private static final float STRIP_LEFT_MARGIN = 2;
    private static final float STRIP_RIGHT_MARGIN = 2;
    private static final float STRIP_WIDTH = 200;
    private static final float BUTTON_WIDTH = 10;
    private static final float TOUCH_OFFSET = 5;

    private StripStacker mTarget = new DummyStacker();

    @Mock
    private StripLayoutTab mTab1;
    @Mock
    private StripLayoutTab mTab2;
    @Mock
    private StripLayoutTab mTab3;
    @Mock
    private StripLayoutTab mTab4;
    @Mock
    private StripLayoutTab mTab5;
    private StripLayoutTab[] mInput;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mInput = new StripLayoutTab[] {mTab1, mTab2, mTab3, mTab4, mTab5};
        float x = 0;
        for (StripLayoutTab tab : mInput) {
            when(tab.getWidth()).thenReturn(TAB_WIDTH);
            when(tab.getWidthWeight()).thenReturn(TAB_WEIGHT);
            when(tab.getDrawX()).thenReturn(x);
            x += TAB_WIDTH;
        }
        setTabStripImprovementFeature(false);
    }

    @After
    public void tearDown() {
        setTabStripImprovementFeature(false);
    }

    @Test
    public void testCreateVisualOrdering() {
        final StripLayoutTab[] output = new StripLayoutTab[mInput.length];
        final StripLayoutTab[] expected_output =
                new StripLayoutTab[] {mTab1, mTab2, mTab5, mTab4, mTab3};

        mTarget.createVisualOrdering(2, mInput, output);
        assertThat("Visual ordering does not match", output, equalTo(expected_output));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/1385702")
    public void testComputeNewTabButtonOffset() {
        float result = mTarget.computeNewTabButtonOffset(mInput, TAB_OVERLAP, STRIP_LEFT_MARGIN,
                STRIP_RIGHT_MARGIN, STRIP_WIDTH, BUTTON_WIDTH, TOUCH_OFFSET, CACHED_TAB_WIDTH,
                true);
        assertThat("New Tab button offset does not match", result, is(130f));
    }

    @Test
    @DisabledTest(message = "https://crbug.com/1385702")
    public void testComputeNewTabButtonOffset_withTabStripImprovements() {
        setTabStripImprovementFeature(true);
        float result = mTarget.computeNewTabButtonOffset(mInput, TAB_OVERLAP, STRIP_LEFT_MARGIN,
                STRIP_RIGHT_MARGIN, STRIP_WIDTH, BUTTON_WIDTH, TOUCH_OFFSET, CACHED_TAB_WIDTH,
                true);
        assertThat("New Tab button offset does not match", result, is(35f));
    }

    @Test
    public void testComputeNewTabButtonOffsetRTL() {
        LocalizationUtils.setRtlForTesting(true);
        float expected_res = 3f;
        // Update drawX for RTL = ((mInput.length -1 ) * TAB_WIDTH) + TOUCH_OFFSET + BUTTON_WIDTH
        // +expected_res = 4*25 + 5 + 10 +3
        float draw_x = 118f;
        for (StripLayoutTab tab : mInput) {
            when(tab.getDrawX()).thenReturn(draw_x);
            draw_x -= TAB_WIDTH;
        }
        float result = mTarget.computeNewTabButtonOffset(mInput, TAB_OVERLAP, STRIP_LEFT_MARGIN,
                STRIP_RIGHT_MARGIN, STRIP_WIDTH, BUTTON_WIDTH, TOUCH_OFFSET, CACHED_TAB_WIDTH,
                true);
        assertThat("New Tab button offset does not match", result, is(expected_res));
    }

    @Test
    public void testComputeNewTabButtonOffsetRTL_withTabStripImprovements() {
        LocalizationUtils.setRtlForTesting(true);
        setTabStripImprovementFeature(true);
        float expected_res = 3f;
        // Update idealX for RTL = ((mInput.length -1 ) * TAB_WIDTH) + TOUCH_OFFSET + BUTTON_WIDTH +
        // expected_res = 4*25 + 5 + 10 + 3
        float ideal_x = 118f;
        for (StripLayoutTab tab : mInput) {
            when(tab.getIdealX()).thenReturn(ideal_x);
            ideal_x -= TAB_WIDTH;
        }
        float result = mTarget.computeNewTabButtonOffset(mInput, TAB_OVERLAP, STRIP_LEFT_MARGIN,
                STRIP_RIGHT_MARGIN, STRIP_WIDTH, BUTTON_WIDTH, TOUCH_OFFSET, CACHED_TAB_WIDTH,
                true);
        assertThat("New Tab button offset does not match", result, is(expected_res));
    }

    private void setTabStripImprovementFeature(boolean value) {
        ChromeFeatureList.sTabStripImprovements.setForTesting(value);
    }

    class DummyStacker extends StripStacker {
        @Override
        public void setTabOffsets(int selectedIndex, StripLayoutTab[] indexOrderedTabs,
                float tabStackWidth, int maxTabsToStack, float tabOverlapWidth,
                float stripLeftMargin, float stripRightMargin, float stripWidth,
                boolean inReorderMode, boolean tabClosing, float cachedTabWidth) {}

        @Override
        public void performOcclusionPass(
                int selectedIndex, StripLayoutTab[] indexOrderedTabs, float stripWidth) {}
    }
}
