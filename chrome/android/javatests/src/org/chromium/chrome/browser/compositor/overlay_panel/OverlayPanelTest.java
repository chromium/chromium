// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlay_panel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.overlay_panel.PanelState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests logic in the OverlayPanel. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@NullMarked
public class OverlayPanelTest {
    private static final int MOCK_VIEWPORT_WIDTH = 400;
    private static final int MOCK_VIEWPORT_HEIGHT = 1000;
    private static final int MOCK_VIEWPORT_OFFSET_Y = 100;
    private static final int MOCK_TOOLBAR_HEIGHT = 100;

    private static final float MOCK_PEEKED_HEIGHT = 200.0f;
    private static final float MOCK_EXPANDED_HEIGHT = 400.0f;
    private static final float MOCK_MAXIMIZED_HEIGHT = 600.0f;

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
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private OverlayPanelContent mMockContent;

    @SuppressWarnings("NullAway.Init")
    Activity mActivity;

    @SuppressWarnings("NullAway.Init")
    ActivityWindowAndroid mWindowAndroid;

    @SuppressWarnings("NullAway.Init")
    TestOverlayPanel mPanel;

    private static class TestOverlayPanel extends OverlayPanel {
        private final OverlayPanelContent mContent;
        private int mOnLayoutChangedCallCount;
        private int mResizePanelContentViewCallCount;

        public TestOverlayPanel(
                Context context,
                LayoutManagerImpl layoutManager,
                OverlayPanelManager panelManager,
                BrowserControlsStateProvider browserControlsStateProvider,
                WindowAndroid windowAndroid,
                Profile profile,
                ViewGroup compositorViewHolder,
                Tab tab,
                OverlayPanelContent content,
                DesktopWindowStateManager desktopWindowStateManager,
                BottomControlsStacker bottomControlsStacker) {
            super(
                    context,
                    layoutManager,
                    panelManager,
                    browserControlsStateProvider,
                    windowAndroid,
                    profile,
                    compositorViewHolder,
                    MOCK_TOOLBAR_HEIGHT,
                    () -> tab,
                    desktopWindowStateManager,
                    bottomControlsStacker);
            mContent = content;
        }

        @Override
        public float getPanelHeightFromState(@Nullable @PanelState Integer state) {
            if (state == null) return 0.0f;
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

        @Override
        public OverlayPanelContent getOverlayPanelContent() {
            return mContent;
        }

        @Override
        public boolean onLayoutChanged(float width, float height, float visibleViewportOffsetY) {
            mOnLayoutChangedCallCount++;
            return super.onLayoutChanged(width, height, visibleViewportOffsetY);
        }

        @Override
        public void resizePanelContentView() {
            mResizePanelContentViewCallCount++;
            super.resizePanelContentView();
        }

        public int getOnLayoutChangedCallCount() {
            return mOnLayoutChangedCallCount;
        }

        public int getResizePanelContentViewCallCount() {
            return mResizePanelContentViewCallCount;
        }

        public void resetCallCounts() {
            mOnLayoutChangedCallCount = 0;
            mResizePanelContentViewCallCount = 0;
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
                                    mInsetObserver,
                                    /* occlusionTrackingAllowed= */ true);
                    OverlayPanelManager panelManager = new OverlayPanelManager();
                    mPanel =
                            new TestOverlayPanel(
                                    mActivity,
                                    mLayoutManager,
                                    panelManager,
                                    mBrowserControlsStateProvider,
                                    mWindowAndroid,
                                    mProfile,
                                    mCompositorViewHolder,
                                    mTab,
                                    mMockContent,
                                    mDesktopWindowStateManager,
                                    mBottomControlsStacker);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid.destroy();
                });
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testOnSizeChanged_NotShowing() {
        mPanel.setHeightForTesting(0.0f);
        mPanel.onSizeChanged(
                MOCK_VIEWPORT_WIDTH,
                MOCK_VIEWPORT_HEIGHT,
                MOCK_VIEWPORT_OFFSET_Y,
                Layout.Orientation.PORTRAIT);

        assertEquals(MOCK_VIEWPORT_WIDTH, mPanel.getViewportWidth(), 0.1f);
        assertEquals(MOCK_VIEWPORT_HEIGHT, mPanel.getViewportHeight(), 0.1f);
        assertEquals(1, mPanel.getOnLayoutChangedCallCount());
        assertEquals(1, mPanel.getResizePanelContentViewCallCount());
        verify(mMockContent, never()).setContentViewSize(anyInt(), anyInt(), anyBoolean());
        verify(mMockContent, never()).resizePanelContentView();
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testOnSizeChanged_Showing() {
        mPanel.setHeightForTesting(MOCK_PEEKED_HEIGHT);
        mPanel.onSizeChanged(
                MOCK_VIEWPORT_WIDTH,
                MOCK_VIEWPORT_HEIGHT,
                MOCK_VIEWPORT_OFFSET_Y,
                Layout.Orientation.PORTRAIT);

        assertEquals(MOCK_VIEWPORT_WIDTH, mPanel.getViewportWidth(), 0.1f);
        assertEquals(MOCK_VIEWPORT_HEIGHT, mPanel.getViewportHeight(), 0.1f);
        assertEquals(1, mPanel.getOnLayoutChangedCallCount());
        assertEquals(1, mPanel.getResizePanelContentViewCallCount());
        verify(mMockContent, atLeastOnce()).resizePanelContentView();
        verify(mMockContent, atLeastOnce()).setContentViewSize(anyInt(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testOnSizeChanged_NoChange() {
        mPanel.setHeightForTesting(0.0f);

        // Initial call to set dimensions.
        mPanel.onSizeChanged(
                MOCK_VIEWPORT_WIDTH,
                MOCK_VIEWPORT_HEIGHT,
                MOCK_VIEWPORT_OFFSET_Y,
                Layout.Orientation.PORTRAIT);

        assertEquals(MOCK_VIEWPORT_WIDTH, mPanel.getViewportWidth(), 0.1f);
        assertEquals(MOCK_VIEWPORT_HEIGHT, mPanel.getViewportHeight(), 0.1f);
        assertEquals(1, mPanel.getOnLayoutChangedCallCount());
        assertEquals(1, mPanel.getResizePanelContentViewCallCount());

        mPanel.resetCallCounts();

        // Second call with same dimensions.
        mPanel.onSizeChanged(
                MOCK_VIEWPORT_WIDTH,
                MOCK_VIEWPORT_HEIGHT,
                MOCK_VIEWPORT_OFFSET_Y,
                Layout.Orientation.PORTRAIT);

        assertEquals(1, mPanel.getOnLayoutChangedCallCount());
        assertEquals(0, mPanel.getResizePanelContentViewCallCount());
    }

    @Test
    @SmallTest
    @Feature({"OverlayPanelBase"})
    @UiThreadTest
    public void testOnSizeChanged_OrientationChange() {
        mPanel.setHeightForTesting(0.0f);

        // Initial call to set dimensions.
        mPanel.onSizeChanged(
                MOCK_VIEWPORT_WIDTH,
                MOCK_VIEWPORT_HEIGHT,
                MOCK_VIEWPORT_OFFSET_Y,
                Layout.Orientation.PORTRAIT);

        assertEquals(MOCK_VIEWPORT_WIDTH, mPanel.getViewportWidth(), 0.1f);
        assertEquals(MOCK_VIEWPORT_HEIGHT, mPanel.getViewportHeight(), 0.1f);
        assertEquals(1, mPanel.getOnLayoutChangedCallCount());
        assertEquals(1, mPanel.getResizePanelContentViewCallCount());

        mPanel.resetCallCounts();

        // Second call with different dimensions.
        mPanel.onSizeChanged(
                MOCK_VIEWPORT_HEIGHT,
                MOCK_VIEWPORT_WIDTH,
                MOCK_VIEWPORT_OFFSET_Y,
                Layout.Orientation.LANDSCAPE);

        assertEquals(MOCK_VIEWPORT_HEIGHT, mPanel.getViewportWidth(), 0.1f);
        assertEquals(MOCK_VIEWPORT_WIDTH, mPanel.getViewportHeight(), 0.1f);
        assertEquals(1, mPanel.getOnLayoutChangedCallCount());
        assertEquals(1, mPanel.getResizePanelContentViewCallCount());
    }
}
