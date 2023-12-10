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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
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

    @Mock private BrowserControlsSizer mBrowserControlsSizer;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsHeight();
        mMediator = new MiniPlayerMediator(mBrowserControlsSizer);
        verify(mBrowserControlsSizer).addObserver(mBrowserControlsObserverCaptor.capture());
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
        verify(mBrowserControlsSizer).removeObserver(eq(mBrowserControlsObserverCaptor.getValue()));
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
        assertEquals(View.INVISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Simulate the layout reporting its height.
        mMediator.onHeightKnown(HEIGHT_PX);
        // Bottom controls resize should be triggered.
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(eq(false));
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX));
        assertEquals(HEIGHT_PX, mModel.get(Properties.HEIGHT));

        // Simulate the bottom controls being resized instantly.
        onControlsOffsetChanged(0, HEIGHT_PX, false);

        // Layout should become visible.
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
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
        assertEquals(View.INVISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Simulate the layout reporting its height.
        mMediator.onHeightKnown(HEIGHT_PX);
        // Bottom controls resize should be triggered.
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(eq(true));
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX));
        assertEquals(HEIGHT_PX, mModel.get(Properties.HEIGHT));

        // Simulate the bottom controls being resized over a few externally driven
        // animation steps.
        onControlsOffsetChanged(-HEIGHT_PX, 0, true);
        onControlsOffsetChanged(-2 * HEIGHT_PX / 3, HEIGHT_PX / 3, true);
        onControlsOffsetChanged(-HEIGHT_PX / 3, 2 * HEIGHT_PX / 3, true);

        // Make sure the next step doesn't start until resizing finishes.
        assertEquals(View.INVISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));

        // Browser controls reach their final height.
        onControlsOffsetChanged(0, HEIGHT_PX, true);

        // Layout should become visible.
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
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
    public void testDismissImmediately() {
        // Show once.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        mMediator.onFullOpacityReached();
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());

        reset(mBrowserControlsSizer);
        doReturn(HEIGHT_PX).when(mBrowserControlsSizer).getBottomControlsHeight();

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
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(eq(false));
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(0), eq(0));

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

        reset(mBrowserControlsSizer);
        doReturn(HEIGHT_PX).when(mBrowserControlsSizer).getBottomControlsHeight();

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
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(eq(true));
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(0), eq(0));

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

        reset(mBrowserControlsSizer);
        doReturn(HEIGHT_PX).when(mBrowserControlsSizer).getBottomControlsHeight();

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
    public void testShowWithHeightAlreadyKnown() {
        // Show once.
        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        mMediator.onFullOpacityReached();

        // Dismiss.
        mMediator.dismiss(true);
        mMediator.onZeroOpacityReached();
        onControlsOffsetChanged(-HEIGHT_PX, 0, true);

        reset(mBrowserControlsSizer);

        // Show again.
        mMediator.show(/* animate= */ false);
        // Bottom controls should grow without a call to onHeightKnown().
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(eq(false));
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(HEIGHT_PX), eq(HEIGHT_PX));
    }

    @Test
    public void testShowWithOtherBottomControls() {
        final int otherBottomControlsHeight = 134;
        final int totalHeight = otherBottomControlsHeight + HEIGHT_PX;
        doReturn(otherBottomControlsHeight).when(mBrowserControlsSizer).getBottomControlsHeight();

        mMediator.show(/* animate= */ true);
        mMediator.onHeightKnown(HEIGHT_PX);

        // Bottom controls resize should take previous height into account.
        verify(mBrowserControlsSizer).setBottomControlsHeight(eq(totalHeight), eq(HEIGHT_PX));

        // Simulate the animated resize.
        onControlsOffsetChanged(-HEIGHT_PX, 0, true);
        onControlsOffsetChanged(-2 * HEIGHT_PX / 3, HEIGHT_PX / 3, true);
        onControlsOffsetChanged(-HEIGHT_PX / 3, 2 * HEIGHT_PX / 3, true);
        assertEquals(View.INVISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        onControlsOffsetChanged(0, HEIGHT_PX, true);
        assertEquals(View.VISIBLE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        assertTrue(mModel.get(Properties.CONTENTS_OPAQUE));
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
}
