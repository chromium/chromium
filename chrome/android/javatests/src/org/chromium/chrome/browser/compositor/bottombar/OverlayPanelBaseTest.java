// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests logic in the OverlayPanelBase. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class OverlayPanelBaseTest {
    private static final float UPWARD_VELOCITY = -1.0f;
    private static final float DOWNWARD_VELOCITY = 1.0f;

    private static final float MOCK_PEEKED_HEIGHT = 200.0f;
    private static final float MOCK_EXPANDED_HEIGHT = 400.0f;
    private static final float MOCK_MAXIMIZED_HEIGHT = 600.0f;

    private static final int MOCK_TOOLBAR_HEIGHT = 100;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ViewGroup mCompositorViewHolder;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private InsetObserver mInsetObserver;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;

    Activity mActivity;
    ActivityWindowAndroid mWindowAndroid;
    MockOverlayPanel mNoExpandPanel;
    MockOverlayPanel mExpandPanel;

    /** Mock OverlayPanel. */
    private static class MockOverlayPanel extends OverlayPanel {
        public MockOverlayPanel(
                Context context,
                LayoutManagerImpl layoutManager,
                OverlayPanelManager manager,
                BrowserControlsStateProvider browserControlsStateProvider,
                WindowAndroid windowAndroid,
                Profile profile,
                ViewGroup compositorViewHolder,
                Tab tab,
                DesktopWindowStateManager desktopWindowStateManager) {
            super(
                    context,
                    layoutManager,
                    manager,
                    browserControlsStateProvider,
                    windowAndroid,
                    profile,
                    compositorViewHolder,
                    MOCK_TOOLBAR_HEIGHT,
                    () -> tab,
                    desktopWindowStateManager);
        }

        /** Expose protected super method as public. */
        @Override
        public @PanelState int findNearestPanelStateFromHeight(
                float desiredHeight, float velocity) {
            return super.findNearestPanelStateFromHeight(desiredHeight, velocity);
        }

        /** Override to return arbitrary test heights. */
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

    /** A MockOverlayPanel that does not support the EXPANDED panel state. */
    private static class NoExpandMockOverlayPanel extends MockOverlayPanel {
        public NoExpandMockOverlayPanel(
                Context context,
                LayoutManagerImpl layoutManager,
                OverlayPanelManager manager,
                BrowserControlsStateProvider browserControlsStateProvider,
                WindowAndroid windowAndroid,
                Profile profile,
                ViewGroup compositorViewHolder,
                Tab tab) {
            super(
                    context,
                    layoutManager,
                    manager,
                    browserControlsStateProvider,
                    windowAndroid,
                    profile,
                    compositorViewHolder,
                    tab,
                    /* desktopWindowStateManager= */ null);
        }

        @Override
        public float getThresholdToNextState() {
            return 0.3f;
        }
    }

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity = activityTestRule.getActivity();
                    mWindowAndroid =
                            new ActivityWindowAndroid(
                                    mActivity,
                                    /* listenToActivityState= */ true,
                                    IntentRequestTracker.createFromActivity(mActivity),
                                    mInsetObserver);
                    OverlayPanelManager panelManager = new OverlayPanelManager();
                    mExpandPanel =
                            new MockOverlayPanel(
                                    mActivity,
                                    mLayoutManager,
                                    panelManager,
                                    mBrowserControlsStateProvider,
                                    mWindowAndroid,
                                    mProfile,
                                    mCompositorViewHolder,
                                    mTab,
                                    mDesktopWindowStateManager);
                    mNoExpandPanel =
                            new NoExpandMockOverlayPanel(
                                    mActivity,
                                    mLayoutManager,
                                    panelManager,
                                    mBrowserControlsStateProvider,
                                    mWindowAndroid,
                                    mProfile,
                                    mCompositorViewHolder,
                                    mTab);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid.destroy();
                });
    }

    // Start OverlayPanelBase test suite.

    /**
     * Tests that a panel will move to the correct state based on current position and swipe
     * velocity.
     */
    @Test
    @MediumTest
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
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(peekToExpBound - 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.PEEKED);

        // Between PEEKING and EXPANDED before the threshold in the down direction.
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(expToPeekBound + 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between PEEKING and EXPANDED past the threshold in the down direction.
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(expToPeekBound - 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.PEEKED);

        // Between EXPANDED and MAXIMIZED past the threshold in the up direction.
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(expToMaxBound + 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);

        // Between EXPANDED and MAXIMIZED before the threshold in the up direction.
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(expToMaxBound - 1, UPWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between EXPANDED and MAXIMIZED past the threshold in the down direction.
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(maxToExpBound - 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.EXPANDED);

        // Between EXPANDED and MAXIMIZED before the threshold in the down direction.
        nextState =
                mExpandPanel.findNearestPanelStateFromHeight(maxToExpBound + 1, DOWNWARD_VELOCITY);
        Assert.assertTrue(nextState == PanelState.MAXIMIZED);
    }

    /** Tests that panel's maximum size accounts for app header height. */
    @Test
    @SmallTest
    @Feature("DesktopWindow")
    @UiThreadTest
    public void testUpdatePanelSizeAndBasePageOffsetWithAppHeaderHeight() {
        int appHeaderHeight = 80;
        AppHeaderState state =
                new AppHeaderState(
                        new Rect(0, 0, 10, appHeaderHeight),
                        new Rect(0, 0, 5, appHeaderHeight),
                        true);
        float pxToDp = 1.f / mActivity.getResources().getDisplayMetrics().density;

        // Set layout params.
        int layoutHeight = 400;
        mExpandPanel.onLayoutChanged(400, layoutHeight, 100);
        mExpandPanel.setIsFullWidthSizePanelForTesting(true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(state);

        // With app header.
        mExpandPanel.onAppHeaderStateChanged(state);
        // Update base page targetY to account for DW state.
        mExpandPanel.updateBasePageTargetY();

        // Set panel height - expanded. Verify base page did not offset.
        mExpandPanel.setPanelHeight(mExpandPanel.getExpandedHeight() - 10);
        float expectedExpandedHeight =
                (mExpandPanel.getTabHeight() - (appHeaderHeight * pxToDp)) * .7f;
        Assert.assertEquals(
                "Expanded panel is not at expected height with app header",
                expectedExpandedHeight,
                mExpandPanel.getExpandedHeight(),
                MathUtils.EPSILON);
        Assert.assertEquals(
                "BasePageY should not be set with app header",
                0,
                mExpandPanel.getBasePageY(),
                MathUtils.EPSILON);

        // Set panel height - maximized. Verify base page did not offset.
        mExpandPanel.setPanelHeight(layoutHeight + 10);
        float expectedMaxHeight = mExpandPanel.getTabHeight() - (appHeaderHeight * pxToDp);
        Assert.assertEquals(
                "Maximized panel is not at expected height with app header",
                expectedMaxHeight,
                mExpandPanel.getMaximizedHeight(),
                MathUtils.EPSILON);
        Assert.assertEquals(
                "BasePageY should not be set with app header",
                0,
                mExpandPanel.getBasePageY(),
                MathUtils.EPSILON);

        // Test without app header
        mExpandPanel.onAppHeaderStateChanged(new AppHeaderState());
        // Update base page targetY to account for DW state.
        mExpandPanel.updateBasePageTargetY();

        // Set panel height - expanded. Verify base page did offset.
        mExpandPanel.setPanelHeight(mExpandPanel.getExpandedHeight() - 10);
        Assert.assertEquals(
                "Expanded panel is not at expected height without app header",
                (mExpandPanel.getTabHeight() * .7f),
                mExpandPanel.getExpandedHeight(),
                MathUtils.EPSILON);
        Assert.assertNotEquals(
                "BasePageY should be set with app header",
                0,
                mExpandPanel.getBasePageY(),
                MathUtils.EPSILON);

        // Set panel height - maximized. Verify base page did offset.
        mExpandPanel.setPanelHeight(layoutHeight + 10);
        Assert.assertEquals(
                "Maximized panel is not at expected height without app header",
                mExpandPanel.getTabHeight(),
                mExpandPanel.getMaximizedHeight(),
                MathUtils.EPSILON);
        Assert.assertNotEquals(
                "BasePageY should be set with app header",
                0,
                mExpandPanel.getBasePageY(),
                MathUtils.EPSILON);
    }

    /** Tests that a panel will be closed if the desired height is negative. */
    @Test
    @MediumTest
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

    /** Tests that a panel is only maximized when desired height is far above the max. */
    @Test
    @LargeTest
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
