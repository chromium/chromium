// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests logic in the OverlayPanelBase.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class OverlayPanelBaseTest {
    private static final float UPWARD_VELOCITY = -1.0f;
    private static final float DOWNWARD_VELOCITY = 1.0f;

    private static final float MOCK_PEEKED_HEIGHT = 200.0f;
    private static final float MOCK_EXPANDED_HEIGHT = 400.0f;
    private static final float MOCK_MAXIMIZED_HEIGHT = 600.0f;

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    MockOverlayPanel mNoExpandPanel;
    MockOverlayPanel mExpandPanel;

    /**
     * Mock OverlayPanel.
     */
    private static class MockOverlayPanel extends OverlayPanel {
        public MockOverlayPanel(Context context, OverlayPanelManager manager) {
            super(context, null, manager);
        }

        /**
         * Expose protected super method as public.
         */
        @Override
        public @PanelState int findNearestPanelStateFromHeight(
                float desiredHeight, float velocity) {
            return super.findNearestPanelStateFromHeight(desiredHeight, velocity);
        }

        /**
         * Override to return arbitrary test heights.
         */
        @Override
        public float getPanelHeightFromState(@Nullable @PanelState Integer state) {
            switch (state) {
                case PanelState.PEEKED:
                    return MOCK_PEEKED_HEIGHT;
                case PanelState.EXPANDED:
                    return MOCK_EXPANDED_HEIGHT;
                case PanelState.MAXIMIZED:
                    return MOCK_MAXIMIZED_HEIGHT;
                default:
                    return 0.0f;
            }
        }
    }

    /**
     * A MockOverlayPanel that does not support the EXPANDED panel state.
     */
    private static class NoExpandMockOverlayPanel extends MockOverlayPanel {

        public NoExpandMockOverlayPanel(Context context, OverlayPanelManager manager) {
            super(context, manager);
        }

        @Override
        public float getThresholdToNextState() {
            return 0.3f;
        }

        @Override
        protected boolean isSupportedState(@PanelState int state) {
            return state != PanelState.EXPANDED;
        }
    }

    @Before
    public void setUp() {
        OverlayPanelManager panelManager = new OverlayPanelManager();
        mExpandPanel =
                new MockOverlayPanel(InstrumentationRegistry.getTargetContext(), panelManager);
        mNoExpandPanel = new NoExpandMockOverlayPanel(
                InstrumentationRegistry.getTargetContext(), panelManager);
    }

    // Start OverlayPanelBase test suite.

    /**
     * Tests that a panel with the EXPANDED state disabled and a lower movement threshold will move
     * to the correct state based on current position and swipe velocity.
     */
    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testNonExpandingPanelMovesToCorrectState() {
        final float threshold = mNoExpandPanel.getThresholdToNextState();
        final float height = MOCK_MAXIMIZED_HEIGHT - MOCK_PEEKED_HEIGHT;
        final float peekToMaxBound = threshold * height + MOCK_PEEKED_HEIGHT;
        final float maxToPeekBound = (1.0f - threshold) * height + MOCK_PEEKED_HEIGHT;

        // Between PEEKING and MAXIMIZED past the threshold in the up direction.
        @PanelState
        int nextState =
                mNoExpandPanel.findNearestPanelStateFromHeight(peekToMaxBound + 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        // Between PEEKING and MAXIMIZED before the threshold in the up direction.
        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(
                peekToMaxBound - 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.PEEKED);

        // Between PEEKING and MAXIMIZED before the threshold in the down direction.
        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(
                maxToPeekBound + 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        // Between PEEKING and MAXIMIZED past the threshold in the down direction.
        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(
                maxToPeekBound - 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.PEEKED);
    }

    /**
     * Tests that a panel will move to the correct state based on current position and swipe
     * velocity.
     */
    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testExpandingPanelMovesToCorrectState() {
        final float threshold = mExpandPanel.getThresholdToNextState();
        final float peekToExpHeight = MOCK_EXPANDED_HEIGHT - MOCK_PEEKED_HEIGHT;
        final float expToMaxHeight = MOCK_MAXIMIZED_HEIGHT - MOCK_EXPANDED_HEIGHT;

        // The boundry for moving to the next state will be different depending on the direction
        // of the swipe and the threshold. In the default case, the threshold is 0.5, meaning the
        // the panel must be half way to the next state in order to animate to it. In other cases
        // where the threshold is 0.3, for example, the boundry will be closer to the top when
        // swiping down and closer to the bottom when swiping up. Ultimately this means it will
        // take less effort to swipe to a different state.
        // NOTE(mdjones): Consider making these constants to exclude computation from these tests.
        final float peekToExpBound = threshold * peekToExpHeight + MOCK_PEEKED_HEIGHT;
        final float expToPeekBound = (1.0f - threshold) * peekToExpHeight + MOCK_PEEKED_HEIGHT;
        final float expToMaxBound = threshold * expToMaxHeight + MOCK_EXPANDED_HEIGHT;
        final float maxToExpBound = (1.0f - threshold) * expToMaxHeight + MOCK_EXPANDED_HEIGHT;

        // Between PEEKING and EXPANDED past the threshold in the up direction.
        @PanelState
        int nextState =
                mExpandPanel.findNearestPanelStateFromHeight(peekToExpBound + 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between PEEKING and EXPANDED before the threshold in the up direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                peekToExpBound - 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.PEEKED);

        // Between PEEKING and EXPANDED before the threshold in the down direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                expToPeekBound + 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between PEEKING and EXPANDED past the threshold in the down direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                expToPeekBound - 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.PEEKED);

        // Between EXPANDED and MAXIMIZED past the threshold in the up direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                expToMaxBound + 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        // Between EXPANDED and MAXIMIZED before the threshold in the up direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                expToMaxBound - 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between EXPANDED and MAXIMIZED past the threshold in the down direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                maxToExpBound - 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between EXPANDED and MAXIMIZED before the threshold in the down direction.
        nextState = mExpandPanel.findNearestPanelStateFromHeight(
                maxToExpBound + 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);
    }

    /**
     * Tests that a panel will be closed if the desired height is negative.
     */
    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testNegativeHeightClosesPanel() {
        final float belowPeek = MOCK_PEEKED_HEIGHT - 1000;

        @PanelState
        int nextState = mExpandPanel.findNearestPanelStateFromHeight(belowPeek, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.CLOSED);

        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(belowPeek, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.CLOSED);

        // Make sure nothing bad happens if velocity is upward (this should never happen).
        nextState = mExpandPanel.findNearestPanelStateFromHeight(belowPeek, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.CLOSED);

        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(belowPeek, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.CLOSED);
    }

    /**
     * Tests that a panel is only maximized when desired height is far above the max.
     */
    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testLargeDesiredHeightIsMaximized() {
        final float aboveMax = MOCK_MAXIMIZED_HEIGHT + 1000;

        @PanelState
        int nextState = mExpandPanel.findNearestPanelStateFromHeight(aboveMax, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(aboveMax, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        // Make sure nothing bad happens if velocity is downward (this should never happen).
        nextState = mExpandPanel.findNearestPanelStateFromHeight(aboveMax, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        nextState = mNoExpandPanel.findNearestPanelStateFromHeight(aboveMax, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);
    }
}
