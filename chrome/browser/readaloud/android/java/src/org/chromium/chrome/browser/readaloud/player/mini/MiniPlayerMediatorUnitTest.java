// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerMediatorUnitTest {
    private static final int HEIGHT_PX = 187;

    private PropertyModel mModel;
    private MiniPlayerMediator mMediator;

    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private MiniPlayerCoordinator mCoordinator;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(0).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        mMediator = new MiniPlayerMediator(mBottomControlsStacker);
        mMediator.setCoordinator(mCoordinator);
        verify(mBrowserControlsStateProvider).addObserver(mBrowserControlsObserverCaptor.capture());
        mModel = mMediator.getModel();
    }

    @Test
    public void testInitialModelState() {
        assertEquals(VisibilityState.GONE, mModel.get(Properties.VISIBILITY));
        assertEquals(View.GONE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        assertFalse(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(mMediator, mModel.get(Properties.MEDIATOR));
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mBrowserControlsStateProvider)
                .removeObserver(eq(mBrowserControlsObserverCaptor.getValue()));
    }

    @Test
    public void testShowAlreadyVisible() {
        mModel.set(Properties.VISIBILITY, VisibilityState.VISIBLE);
        mMediator.show(/* animate= */ false);
        // VISIBILITY should be unchanged.
        assertEquals(VisibilityState.VISIBLE, mModel.get(Properties.VISIBILITY));
    }

    @Test
    public void testShowImmediately() {
        mMediator.show(/* animate= */ false);

        // Layout visibility, CC layer visibility, and overall VisibilityState should be set.
        assertFalse(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Simulate the layout reporting its height.
        mMediator.onHeightKnown(HEIGHT_PX);
        // Bottom controls resize should be triggered.
        verify(mBottomControlsStacker)
                .setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX), eq(false));
        assertEquals(HEIGHT_PX, mModel.get(Properties.HEIGHT));

        // Simulate the bottom controls being resized instantly.
        onControlsOffsetChanged(0, HEIGHT_PX, false);

        // Layout should become opaque.
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));

        // Simulate the layout calling back after setting opacity.
        mMediator.onFullOpacityReached();
        // Transition is complete.
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());
    }

    @Test
    public void testShowAnimated() {
        mMediator.show(/* animate= */ true);

        // Layout visibility, CC layer visibility, and overall VisibilityState should be set.
        assertTrue(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Simulate the layout reporting its height.
        mMediator.onHeightKnown(HEIGHT_PX);
        // Bottom controls resize should be triggered.
        verify(mBottomControlsStacker)
                .setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX), eq(true));
        assertEquals(HEIGHT_PX, mModel.get(Properties.HEIGHT));

        // Simulate the bottom controls being resized over a few externally driven
        // animation steps.
        onControlsOffsetChanged(-HEIGHT_PX, 0, true);
        onControlsOffsetChanged(-2 * HEIGHT_PX / 3, HEIGHT_PX / 3, true);
        onControlsOffsetChanged(-HEIGHT_PX / 3, 2 * HEIGHT_PX / 3, true);

        // Make sure the next step doesn't start until resizing finishes.
        assertFalse(mModel.get(Properties.CONTENTS_OPAQUE));

        // Browser controls reach their final height.
        onControlsOffsetChanged(0, HEIGHT_PX, true);

        // Layout should become opaque.
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));

        // Simulate the layout calling back after fading in.
        mMediator.onFullOpacityReached();
        // Transition is complete.
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());
    }

    @Test
    public void testShowAlreadyShowing() {
        mMediator.show(/* animate= */ true);
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));

        // Showing again should have no effect on the ongoing transition. Reset
        // COMPOSITED_VIEW_VISIBLE so we can make sure show() doesn't change it.
        mModel.set(Properties.COMPOSITED_VIEW_VISIBLE, false);
        mMediator.show(/* animate= */ true);
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        assertFalse(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
    }

    @Test
    public void testShowWithDelayedRunnable_GrowBottomControlsDoesntAnimate() {
        mMediator.show(/* animate= */ true);

        // Layout visibility, CC layer visibility, and overall VisibilityState should be set.
        assertTrue(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Simulate the layout reporting its height.
        mMediator.onHeightKnown(HEIGHT_PX);
        // Bottom controls resize should be triggered.
        verify(mBottomControlsStacker)
                .setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX), eq(true));
        doReturn(HEIGHT_PX).when(mBrowserControlsStateProvider).getBottomControlsHeight();
        assertEquals(HEIGHT_PX, mModel.get(Properties.HEIGHT));

        // Simulate the bottom controls being resized without an animation
        onBottomControlsHeightChanged(HEIGHT_PX, HEIGHT_PX);
        // onControlsOffsetChanged didn't run, kick in the delayed runnable that
        // will fade in the view;
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Layout should become opaque.
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));

        // Simulate the layout calling back after fading in.
        mMediator.onFullOpacityReached();
        // Transition is complete.
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());
    }

    @Test
    public void testShowWithDelayedRunnable_GrowBottomControlsAnimates() {
        mMediator.show(/* animate= */ true);

        // Layout visibility, CC layer visibility, and overall VisibilityState should be set.
        assertTrue(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Simulate the layout reporting its height.
        mMediator.onHeightKnown(HEIGHT_PX);
        // Bottom controls resize should be triggered.
        verify(mBottomControlsStacker)
                .setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX), eq(true));
        doReturn(HEIGHT_PX).when(mBrowserControlsStateProvider).getBottomControlsHeight();

        assertEquals(HEIGHT_PX, mModel.get(Properties.HEIGHT));

        // Simulate the bottom controls being resized with an animation
        onBottomControlsHeightChanged(HEIGHT_PX, HEIGHT_PX);
        onControlsOffsetChanged(-HEIGHT_PX, HEIGHT_PX, true);
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // The delayed runnable should do nothing, contents should stay opaque
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));
    }

    @Test
    public void testDismissDoestTriggerDelayedRunnable() {
        // Show once.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        mMediator.onFullOpacityReached();
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());

        reset(mBrowserControlsStateProvider);
        doReturn(HEIGHT_PX).when(mBrowserControlsStateProvider).getBottomControlsHeight();

        // Dismiss.
        mMediator.dismiss(/* animate= */ false);
        assertFalse(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
        // Start by fading out.
        assertFalse(mModel.get(Properties.CONTENTS_OPAQUE));

        // Simulate the layout calling back after setting its opacity.
        mMediator.onZeroOpacityReached();

        // Layout should be GONE and bottom controls resizing should be triggered.
        assertEquals(View.GONE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        verify(mBottomControlsStacker).setBottomControlsHeight(eq(1), eq(0), eq(false));

        onBottomControlsHeightChanged(0, 0);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
    }

    @Test
    public void testDismissImmediately() {
        // Show once.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        mMediator.onFullOpacityReached();
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());

        reset(mBrowserControlsStateProvider);
        doReturn(HEIGHT_PX).when(mBrowserControlsStateProvider).getBottomControlsHeight();

        // Dismiss.
        mMediator.dismiss(/* animate= */ false);
        assertFalse(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
        // Start by fading out.
        assertFalse(mModel.get(Properties.CONTENTS_OPAQUE));

        // Simulate the layout calling back after setting its opacity.
        mMediator.onZeroOpacityReached();

        // Layout should be GONE and bottom controls resizing should be triggered.
        assertEquals(View.GONE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        verify(mBottomControlsStacker).setBottomControlsHeight(eq(1), eq(0), eq(false));

        // Simulate the bottom controls being resized instantly.
        onControlsOffsetChanged(0, 0, false);

        // Transition is complete.
        assertFalse(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.GONE, mMediator.getVisibility());
    }

    @Test
    public void testDismissAnimated() {
        // Show once.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        mMediator.onFullOpacityReached();
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());

        reset(mBrowserControlsStateProvider);
        reset(mBottomControlsStacker);
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        doReturn(HEIGHT_PX).when(mBrowserControlsStateProvider).getBottomControlsHeight();

        // Dismiss.
        mMediator.dismiss(/* animate= */ true);
        assertTrue(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
        // Start by fading out.
        assertFalse(mModel.get(Properties.CONTENTS_OPAQUE));

        // Simulate the layout calling back after fading out.
        mMediator.onZeroOpacityReached();

        // Layout should be GONE and bottom controls resizing should be triggered.
        assertEquals(View.GONE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        verify(mBottomControlsStacker).setBottomControlsHeight(eq(1), eq(0), eq(true));

        // Simulate the bottom controls being resized over a few externally driven
        // animation steps.
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        onControlsOffsetChanged(-HEIGHT_PX / 3, 2 * HEIGHT_PX / 3, true);
        onControlsOffsetChanged(-2 * HEIGHT_PX / 3, HEIGHT_PX / 3, true);

        // Make sure the next step doesn't start until resizing finishes.
        assertTrue(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));

        // Browser controls reach their final height.
        onControlsOffsetChanged(-HEIGHT_PX, 0, true);

        // Transition is complete.
        assertFalse(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(VisibilityState.GONE, mMediator.getVisibility());
    }

    @Test
    public void testDismissAlreadyHiding() {
        // Show once.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        mMediator.onFullOpacityReached();
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());

        reset(mBrowserControlsStateProvider);
        doReturn(HEIGHT_PX).when(mBrowserControlsStateProvider).getBottomControlsHeight();

        // Dismiss.
        mMediator.dismiss(/* animate= */ true);
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
        assertFalse(mModel.get(Properties.CONTENTS_OPAQUE));
        mMediator.onZeroOpacityReached();

        // Dismissing again should have no effect. Reset CONTENTS_OPAQUE to make sure dismiss()
        // doesn't change it.
        mModel.set(Properties.CONTENTS_OPAQUE, true);
        mMediator.dismiss(/* animate= */ true);
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));
    }

    @Test
    public void testShowWithOtherBottomControls() {
        final int otherBottomControlsHeight = 134;
        final int totalHeight = otherBottomControlsHeight + HEIGHT_PX;
        doReturn(otherBottomControlsHeight)
                .when(mBrowserControlsStateProvider)
                .getBottomControlsHeight();

        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Bottom controls resize should take previous height into account.
        verify(mBottomControlsStacker)
                .setBottomControlsHeight(eq(totalHeight), eq(HEIGHT_PX), eq(true));

        // Simulate the animated resize.
        onControlsOffsetChanged(-HEIGHT_PX, 0, true);
        onControlsOffsetChanged(-2 * HEIGHT_PX / 3, HEIGHT_PX / 3, true);
        onControlsOffsetChanged(-HEIGHT_PX / 3, 2 * HEIGHT_PX / 3, true);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));
    }

    @Test
    public void testNotifyShown() {
        // Show once to store height.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, false);
        mMediator.onFullOpacityReached();
        verify(mCoordinator).onShown();
    }

    // TODO hide during show, show during hide

    @Test
    public void testOnBackgroundColorUpdated() {
        mMediator.onBackgroundColorUpdated(0xAABBCCDD);
        assertEquals(0xAABBCCDD, mModel.get(Properties.BACKGROUND_COLOR_ARGB));
    }

    private void onControlsOffsetChanged(
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        mBrowserControlsObserverCaptor
                .getValue()
                .onControlsOffsetChanged(
                        /* topOffset= */ 0,
                        /* topControlsMinHeightOffset= */ 0,
                        bottomOffset,
                        bottomControlsMinHeightOffset,
                        needsAnimate);
    }

    private void onBottomControlsHeightChanged(
            int bottomControlContainerHeight, int bottomControlMinHeight) {
        mBrowserControlsObserverCaptor
                .getValue()
                .onBottomControlsHeightChanged(
                        bottomControlContainerHeight, bottomControlMinHeight);
    }
}
