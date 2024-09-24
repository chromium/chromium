// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackScroller;

/** Tests for {@link ScrollDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScrollDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final float STRIP_WIDTH = 150.f;
    private static final float LEFT_MARGIN = 5.f;
    private static final float RIGHT_MARGIN = 5.f;
    private static final float VIEW_WIDTH = 110.f;
    private static final float VIEW_OVERLAP_WIDTH = 10.f;
    private static final float REORDER_START_MARGIN = 10.f;
    private static final float TRAILING_MARGIN_WIDTH = 10.f;

    private static final float TEST_MIN_SCROLL_OFFSET = -200.f;
    private static final long TIMESTAMP = 0;

    private final Context mContext = RuntimeEnvironment.systemContext;
    private final ScrollDelegate mScrollDelegate = new ScrollDelegate();

    @Mock private StripLayoutGroupTitle mGroupTitle;
    @Mock private StripLayoutTab mTab1;
    @Mock private StripLayoutTab mTab2;
    @Mock private StripLayoutTab mTab3;
    @Mock private StripLayoutTab mTab4;
    private StripLayoutView[] mViews;

    @Before
    public void setup() {
        mScrollDelegate.onContextChanged(mContext);
        mScrollDelegate.setMinScrollOffsetForTesting(TEST_MIN_SCROLL_OFFSET);
    }

    @Test
    public void testSetClampedScrollOffset_LessThan() {
        // Clamping between -200 and 0. Expect to clamp -300 to -200.
        float newScrollOffset = -300.f;
        mScrollDelegate.setScrollOffset(newScrollOffset);
        assertEquals(
                /* message= */ "Offset should be clamped.",
                TEST_MIN_SCROLL_OFFSET,
                mScrollDelegate.getScrollOffset(),
                /* delta= */ 0);
    }

    @Test
    public void testSetClampedScrollOffset_InBounds() {
        // Clamping between -200 and 0. Expect to not clamp -100.
        float newScrollOffset = -100.f;
        mScrollDelegate.setScrollOffset(newScrollOffset);
        assertEquals(
                /* message= */ "Offset should not be clamped.",
                newScrollOffset,
                mScrollDelegate.getScrollOffset(),
                /* delta= */ 0);
    }

    @Test
    public void testSetClampedScrollOffset_GreaterThan() {
        // Clamping between -200 and 0. Expect to clamp 100 to 0.
        float newScrollOffset = 100.f;
        mScrollDelegate.setScrollOffset(newScrollOffset);
        assertEquals(
                /* message= */ "Offset should be clamped.",
                /* expected= */ 0,
                mScrollDelegate.getScrollOffset(),
                /* delta= */ 0);
    }

    @Test
    public void testUpdateScrollInProgress_Finished() {
        assertFalse(
                /* message= */ "No scroll should be in progress.",
                mScrollDelegate.updateScrollInProgress(TIMESTAMP));
    }

    @Test
    public void testUpdateScrollInProgress_NotFinished() {
        // Fake scroll in progress.
        StackScroller scroller = mScrollDelegate.getScrollerForTesting();
        scroller.forceFinished(false);

        // Set un-clamped scrollOffset.
        float scrollOffset = 100.f;
        mScrollDelegate.setNonClampedScrollOffsetForTesting(scrollOffset);
        assertEquals(
                /* message= */ "ScrollOffset should be un-clamped.",
                /* expected= */ scrollOffset,
                mScrollDelegate.getScrollOffset(),
                /* delta= */ 0);

        // Verify scroll state.
        assertTrue(
                /* message= */ "Scroll should be in progress.",
                mScrollDelegate.updateScrollInProgress(TIMESTAMP));
        assertEquals(
                /* message= */ "Should clamp scrollOffset.",
                /* expected= */ 0,
                mScrollDelegate.getScrollOffset(),
                /* delta= */ 0);
    }

    @Test
    public void testUpdateScrollOffsetLimits() {
        // Setup mocks.
        mViews = new StripLayoutView[] {mGroupTitle, mTab1, mTab2, mTab3, mTab4};
        for (StripLayoutView view : mViews) {
            when(view.getWidth()).thenReturn(VIEW_WIDTH);
        }
        // Tab 2 has a trailing margin.
        when(mTab2.getTrailingMargin()).thenReturn(TRAILING_MARGIN_WIDTH);
        // Tabs 3 and 4 will be ignored.
        when(mTab3.isClosed()).thenReturn(true);
        when(mTab4.isDraggedOffStrip()).thenReturn(true);

        // stripWidth = width(150) - leftMargin(5) - rightMargin(5) = 140
        // viewsWidth = 3*(viewWidth(110) - overlapWidth(10)) + overlapWidth(10) = 310
        // marginsWidth = trailingMarginWidth(10) + reorderStartMargin(10) = 20
        // expectedMinScrollOffset = viewsWidth(310) + marginsWidth(20) - stripWidth(140) = -190
        mScrollDelegate.setReorderStartMargin(REORDER_START_MARGIN);
        mScrollDelegate.updateScrollOffsetLimits(
                mViews,
                STRIP_WIDTH,
                LEFT_MARGIN,
                RIGHT_MARGIN,
                VIEW_WIDTH,
                VIEW_OVERLAP_WIDTH,
                VIEW_OVERLAP_WIDTH);
        float expectedMinScrollOffset = -190.f;
        assertEquals(
                /* message= */ "minScrollOffset was not as calculated.",
                expectedMinScrollOffset,
                mScrollDelegate.getMinScrollOffsetForTesting(),
                /* delta= */ 0);
    }

    @Test
    public void testScrollDuration_Short() {
        // A "short" scroll distance has an absolute value less than 960.
        float scrollDelta = -500.f;

        // Verify the "short" scroll duration is 250.
        int expectedScrollDuration = 250;
        assertEquals(
                /* message= */ "Expected a different scroll duration for the given distance,",
                expectedScrollDuration,
                mScrollDelegate.getScrollDuration(scrollDelta));
    }

    @Test
    public void testScrollDuration_Medium() {
        // A "medium" scroll distance has an absolute value between than 960 and 1920.
        float scrollDelta = 1500.f;

        // Verify the "medium" scroll duration is 350.
        int expectedScrollDuration = 350;
        assertEquals(
                /* message= */ "Expected a different scroll duration for the given distance,",
                expectedScrollDuration,
                mScrollDelegate.getScrollDuration(scrollDelta));
    }

    @Test
    public void testScrollDuration_Long() {
        // A "long" scroll distance has an absolute value greater than 1920.
        float scrollDelta = -2500.f;

        // Verify the "long" scroll duration is 450.
        int expectedScrollDuration = 450;
        assertEquals(
                /* message= */ "Expected a different scroll duration for the given distance,",
                expectedScrollDuration,
                mScrollDelegate.getScrollDuration(scrollDelta));
    }
}
