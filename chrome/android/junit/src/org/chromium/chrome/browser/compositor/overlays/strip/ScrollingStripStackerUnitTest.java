// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link ScrollingStripStacker}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class ScrollingStripStackerUnitTest {
    private static final float TAB_OFFSET_Y = 2;
    private static final float TAB_WIDTH = 25;
    private static final float CACHED_TAB_WIDTH = 30;

    private ScrollingStripStacker mTarget = new ScrollingStripStacker();
    @Mock private StripLayoutTab mTab1;
    @Mock private StripLayoutTab mTab2;
    @Mock private StripLayoutTab mTab3;
    @Mock private StripLayoutTab mTab4;
    @Mock private StripLayoutTab mTab5;
    private StripLayoutTab[] mInput;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mInput = new StripLayoutTab[] {mTab1, mTab2, mTab3, mTab4, mTab5};
        float ideal_x = 0;
        // First and last tab out of visible area.
        float draw_x = -TAB_WIDTH - 1;
        for (StripLayoutTab tab : mInput) {
            when(tab.getIdealX()).thenReturn(ideal_x);
            when(tab.getOffsetY()).thenReturn(TAB_OFFSET_Y);
            when(tab.getWidth()).thenReturn(TAB_WIDTH);
            when(tab.getDrawX()).thenReturn(draw_x);
            draw_x += TAB_WIDTH;
            ideal_x += TAB_WIDTH;
        }
    }

    @Test
    public void testSetTabOffsets_tabNotClosing() {
        mTarget.setViewOffsets(mInput, false, false, false, CACHED_TAB_WIDTH);

        float expected_x = 0;
        for (StripLayoutTab tab : mInput) {
            verify(tab).setDrawX(expected_x);
            verify(tab).setWidth(CACHED_TAB_WIDTH);
            verify(tab).setDrawY(TAB_OFFSET_Y);
            expected_x += TAB_WIDTH;
        }
    }

    @Test
    public void testSetTabOffsets_tabClosing() {
        mTarget.setViewOffsets(mInput, true, false, false, CACHED_TAB_WIDTH);

        for (StripLayoutTab tab : mInput) {
            verify(tab).setDrawY(TAB_OFFSET_Y);
            verify(tab).getOffsetY();
            verifyNoMoreInteractions(tab);
        }
    }

    @Test
    public void testSetTabOffsets_tabCreating() {
        mTarget.setViewOffsets(mInput, false, true, false, CACHED_TAB_WIDTH);

        float expected_x = 0;
        for (StripLayoutTab tab : mInput) {
            verify(tab).getIdealX();
            verify(tab).getOffsetX();
            verify(tab).setDrawX(expected_x);
            verify(tab).setDrawY(TAB_OFFSET_Y);
            verify(tab).getOffsetY();
            verifyNoMoreInteractions(tab);
            expected_x += TAB_WIDTH;
        }
    }

    @Test
    public void testPerformOcclusionPass() {
        mTarget.performOcclusionPass(mInput, 0, 2 * TAB_WIDTH);

        for (StripLayoutTab tab : mInput) {
            if (tab == mTab1 || tab == mTab5) {
                verify(tab).setVisible(false);
            } else {
                verify(tab).setVisible(true);
            }
        }
    }

    @Test
    public void testPerformOcclusionPassWithXOffset() {
        mTarget.performOcclusionPass(mInput, TAB_WIDTH, 2 * TAB_WIDTH);

        // Move the window with xOffset = TAB_WIDTH, will make TAB_2 invisible, but TAB_5 visible.
        for (StripLayoutTab tab : mInput) {
            if (tab == mTab1 || tab == mTab2) {
                verify(tab).setVisible(false);
            } else {
                verify(tab).setVisible(true);
            }
        }
    }

    @Test
    public void testPerformOcclusionPassWithPartialXOffset() {
        mTarget.performOcclusionPass(mInput, TAB_WIDTH / 2, 2 * TAB_WIDTH);

        // Move the window with xOffset = TAB_WIDTH / 2, will make both TAB_2 and TAB_4 partially
        // invisible. TAB_5 will still be invisible in this case.
        for (StripLayoutTab tab : mInput) {
            if (tab == mTab1 || tab == mTab5) {
                verify(tab).setVisible(false);
            } else {
                verify(tab).setVisible(true);
            }
        }
    }
}
