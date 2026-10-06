// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.components.messages.MessageContainer;

/** Unit tests for {@link MessageContainerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageContainerCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsManager mControlsManager;

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
        mCoordinator = new MessageContainerCoordinator(mContainer, mControlsManager);
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
    public void testGetMessageTopOffset_withZeroContentOffsetAndBottomPosition() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.BOTTOM);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(0, offset);
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
