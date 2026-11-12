// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.messages.MessageContainer;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** Unit tests for {@link MessageContainerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageContainerCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsManager mControlsManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();

    private BrowserStateBrowserControlsVisibilityDelegate mVisibilityDelegate;
    private Activity mActivity;
    private MessageContainer mContainer;
    private MessageContainerCoordinator mCoordinator;
    private int mBubbleInset;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContainer = new MessageContainer(mActivity, null);
        mActivity.setContentView(mContainer);
        mBubbleInset =
                mActivity.getResources().getDimensionPixelOffset(R.dimen.message_bubble_inset);
        mVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.createNonNull(false));
        mVisibilityDelegate.set(BrowserControlsState.SHOWN);
        when(mControlsManager.getBrowserVisibilityDelegate()).thenReturn(mVisibilityDelegate);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);
        mCoordinator =
                new MessageContainerCoordinator(
                        mContainer, mControlsManager, mEdgeToEdgeControllerSupplier);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
    }

    /** Adds a focusable child to the container. */
    private View addChild() {
        View child = new View(mActivity);
        child.setFocusable(true);
        child.setFocusableInTouchMode(true);
        // MessageContainer#addView(View) is final and throws; addMessage() is package-private.
        mContainer.addView(child, 0);
        return child;
    }

    @Test
    public void testGetMessageTopOffset_withContentOffset() {
        when(mControlsManager.getContentOffset()).thenReturn(100);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(100 - mBubbleInset, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndTopPosition() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.TOP);
        when(mControlsManager.getTopControlsHeight()).thenReturn(80);
        when(mControlsManager.isVisibilityForced()).thenReturn(true);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(80 - mBubbleInset, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndTopPosition_notForced() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.TOP);
        when(mControlsManager.isVisibilityForced()).thenReturn(false);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(0, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndForcedHidden() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getTopControlsHeight()).thenReturn(80);
        when(mControlsManager.isVisibilityForced()).thenReturn(true);
        mVisibilityDelegate.set(BrowserControlsState.HIDDEN);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(0, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndBottomPosition() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.BOTTOM);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(0, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndBottomPosition_forced() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.BOTTOM);
        when(mControlsManager.getTopControlsHeight()).thenReturn(80);
        when(mControlsManager.isVisibilityForced()).thenReturn(true);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(80 - mBubbleInset, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndBottomPosition_forcedWithScalp() {
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.BOTTOM);
        when(mControlsManager.getTopControlsHeight()).thenReturn(60);
        when(mControlsManager.isVisibilityForced()).thenReturn(true);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(60, offset);
    }

    @Test
    public void testGetMessageTopOffset_withTopInsetOnly_doesNotSubtractBubbleInset() {
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mControlsManager.getContentOffset()).thenReturn(60);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(60, offset);
    }

    @Test
    public void testGetMessageTopOffset_withTopInsetAndScrolledOffScalp() {
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mControlsManager.getContentOffset()).thenReturn(0);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(60, offset);
    }

    @Test
    public void testGetMessageTopOffset_withTopToolbarAndTopInset() {
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mControlsManager.getContentOffset()).thenReturn(160);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(160 - mBubbleInset, offset);
    }

    @Test
    public void testGetMessageTopOffset_clampsToTopInsetWhenContentOffsetSlightlyAboveTopInset() {
        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        when(mControlsManager.getContentOffset()).thenReturn(60 + mBubbleInset / 2);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(60, offset);
    }

    @Test
    public void testOverrideTopInset_updatesVisibleContainerMarginsAndDestroyUnregisters() {
        mContainer.setVisibility(View.VISIBLE);
        when(mControlsManager.getContentOffset()).thenReturn(0);

        mPadAdjusterCaptor.getValue().overrideTopInset(60);
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mContainer.getLayoutParams();
        Assert.assertEquals(60, params.topMargin);

        mCoordinator.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(mPadAdjusterCaptor.getValue());
    }

    @Test
    public void testIsVisible_containerNull() {
        MessageContainerCoordinator coordinator =
                new MessageContainerCoordinator(null, mControlsManager);
        Assert.assertFalse(coordinator.isVisible());
        Assert.assertFalse(coordinator.containsKeyboardFocus());
    }

    @Test
    public void testIsVisible_containerNotVisible() {
        mContainer.setVisibility(View.GONE);
        addChild();
        Assert.assertFalse(mCoordinator.isVisible());
    }

    @Test
    public void testIsVisible_containerVisibleNoChildren() {
        mContainer.setVisibility(View.VISIBLE);
        Assert.assertFalse(mCoordinator.isVisible());
    }

    @Test
    public void testIsVisible_containerVisibleWithChild() {
        mContainer.setVisibility(View.VISIBLE);
        addChild();
        Assert.assertTrue(mCoordinator.isVisible());
    }

    @Test
    public void testContainsKeyboardFocus() {
        View child = addChild();
        Assert.assertFalse(mCoordinator.containsKeyboardFocus());

        Assert.assertTrue(child.requestFocus());
        Assert.assertTrue(mCoordinator.containsKeyboardFocus());
    }

    @Test
    public void testRequestKeyboardFocus_notVisible() {
        // The container is visible but has no message, so isVisible() is false. Make the container
        // itself focusable so that only the coordinator's guard prevents it from taking focus.
        mContainer.setFocusable(true);
        mContainer.setFocusableInTouchMode(true);
        mCoordinator.requestKeyboardFocus();
        Assert.assertFalse(mContainer.hasFocus());
    }

    @Test
    public void testRequestKeyboardFocus_visible() {
        mContainer.setVisibility(View.VISIBLE);
        View child = addChild();

        mCoordinator.requestKeyboardFocus();
        Assert.assertTrue(child.isFocused());
        Assert.assertTrue(mContainer.hasFocus());
    }
}
