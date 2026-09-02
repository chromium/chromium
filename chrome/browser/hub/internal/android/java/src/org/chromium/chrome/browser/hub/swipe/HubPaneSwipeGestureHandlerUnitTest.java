// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;
import android.view.ViewParent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link HubPaneSwipeGestureHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE)
public class HubPaneSwipeGestureHandlerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HubPaneSwipeGestureHandler.SwipeGestureDelegate mDelegate;
    @Mock private ViewParent mViewParent;
    @Mock private VelocityTracker mVelocityTracker;
    @Mock private View mAdjacentView;

    private ActivityController<TestActivity> mActivityController;
    private HubPaneSwipeGestureHandler mGestureHandler;

    private static final int CONTAINER_WIDTH = 1000;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();

        when(mDelegate.getContainerWidth()).thenReturn(CONTAINER_WIDTH);
        mGestureHandler = new HubPaneSwipeGestureHandler(activity, mDelegate);
        mGestureHandler.setVelocityTrackerForTesting(mVelocityTracker);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testSwipeLeft_success() {
        when(mDelegate.onSwipeStarted(true)).thenReturn(mAdjacentView);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 100f; // > 1/3 screen width displacement (400px > 333px)

        doNothing().when(mVelocityTracker).computeCurrentVelocity(anyInt());
        when(mVelocityTracker.getXVelocity()).thenReturn(-1000f);
        when(mVelocityTracker.getYVelocity()).thenReturn(0f);

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(down, mViewParent));
        assertTrue(mGestureHandler.onTouchEvent(down));

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        assertTrue(mGestureHandler.onInterceptTouchEvent(move, mViewParent));
        verify(mViewParent).requestDisallowInterceptTouchEvent(true);
        assertTrue(mGestureHandler.onTouchEvent(move));

        verify(mDelegate).onSwipeStarted(true);
        verify(mDelegate).onSwipeProgress(anyFloat(), anyFloat(), eq(true));

        MotionEvent up =
                MotionEvent.obtain(downTime, downTime + 20, MotionEvent.ACTION_UP, endX, 500f, 0);
        assertTrue(mGestureHandler.onTouchEvent(up));

        verify(mDelegate).onSwipeSettled(eq(true), eq(true));
    }

    @Test
    public void testSwipeRight_success() {
        when(mDelegate.onSwipeStarted(false)).thenReturn(mAdjacentView);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 900f; // > 1/3 screen width displacement

        doNothing().when(mVelocityTracker).computeCurrentVelocity(anyInt());
        when(mVelocityTracker.getXVelocity()).thenReturn(1000f);
        when(mVelocityTracker.getYVelocity()).thenReturn(0f);

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(down, mViewParent);
        mGestureHandler.onTouchEvent(down);

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        assertTrue(mGestureHandler.onInterceptTouchEvent(move, mViewParent));
        mGestureHandler.onTouchEvent(move);

        verify(mDelegate).onSwipeStarted(false);
        verify(mDelegate).onSwipeProgress(anyFloat(), anyFloat(), eq(false));

        MotionEvent up =
                MotionEvent.obtain(downTime, downTime + 20, MotionEvent.ACTION_UP, endX, 500f, 0);
        mGestureHandler.onTouchEvent(up);

        verify(mDelegate).onSwipeSettled(eq(true), eq(false));
    }

    @Test
    public void testSwipe_ignoredInGutter() {
        long downTime = SystemClock.uptimeMillis();
        float startX = mGestureHandler.getSwipeEdgeGutterWidth() / 2f; // In left gutter

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(down, mViewParent));
        assertFalse(mGestureHandler.onTouchEvent(down));

        MotionEvent move =
                MotionEvent.obtain(
                        downTime, downTime + 10, MotionEvent.ACTION_MOVE, startX + 400f, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(move, mViewParent));

        verify(mDelegate, never()).onSwipeStarted(anyBoolean());
    }

    @Test
    public void testSwipe_ignoredOnInteractiveElement() {
        when(mDelegate.isTouchOnInteractiveElement(anyFloat(), anyFloat())).thenReturn(true);

        long downTime = SystemClock.uptimeMillis();
        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, 500f, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(down, mViewParent));
        assertFalse(mGestureHandler.onTouchEvent(down));

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, 100f, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(move, mViewParent));

        verify(mDelegate, never()).onSwipeStarted(anyBoolean());
    }

    @Test
    public void testVerticalMove_doesNotIntercept() {
        long downTime = SystemClock.uptimeMillis();
        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, 500f, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(down, mViewParent);

        // Move purely vertically past touch slop
        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, 500f, 700f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(move, mViewParent));
        assertFalse(mGestureHandler.isSwipeBeingDragged());
    }

    @Test
    public void testFlingInCorrectDirection_triggersSwitch() {
        when(mDelegate.onSwipeStarted(true)).thenReturn(mAdjacentView);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 450f; // Small displacement (50px < 333px threshold)

        doNothing().when(mVelocityTracker).computeCurrentVelocity(anyInt());
        when(mVelocityTracker.getXVelocity()).thenReturn(-2000f); // High leftward fling
        when(mVelocityTracker.getYVelocity()).thenReturn(0f);

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(down, mViewParent);
        mGestureHandler.onTouchEvent(down);

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(move, mViewParent);
        mGestureHandler.onTouchEvent(move);

        MotionEvent up =
                MotionEvent.obtain(downTime, downTime + 20, MotionEvent.ACTION_UP, endX, 500f, 0);
        mGestureHandler.onTouchEvent(up);

        // Should switch because of fling
        verify(mDelegate).onSwipeSettled(eq(true), eq(true));
    }

    @Test
    public void testDirectTouchEvent_whenInterceptNotCalled() {
        when(mDelegate.onSwipeStarted(true)).thenReturn(mAdjacentView);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 100f;

        doNothing().when(mVelocityTracker).computeCurrentVelocity(anyInt());
        when(mVelocityTracker.getXVelocity()).thenReturn(-1000f);
        when(mVelocityTracker.getYVelocity()).thenReturn(0f);

        // Child did not consume ACTION_DOWN, so onTouchEvent is called directly
        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        assertTrue(mGestureHandler.onTouchEvent(down));

        // Subsequent MOVE goes straight to onTouchEvent
        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        assertTrue(mGestureHandler.onTouchEvent(move));

        verify(mDelegate).onSwipeStarted(true);
        verify(mDelegate).onSwipeProgress(anyFloat(), anyFloat(), eq(true));

        MotionEvent up =
                MotionEvent.obtain(downTime, downTime + 20, MotionEvent.ACTION_UP, endX, 500f, 0);
        assertTrue(mGestureHandler.onTouchEvent(up));

        verify(mDelegate).onSwipeSettled(eq(true), eq(true));
    }

    @Test
    public void testSwipe_ignoredInRightGutter() {
        long downTime = SystemClock.uptimeMillis();
        float startX = CONTAINER_WIDTH - (mGestureHandler.getSwipeEdgeGutterWidth() / 2f);

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(down, mViewParent));
        assertFalse(mGestureHandler.onTouchEvent(down));

        MotionEvent move =
                MotionEvent.obtain(
                        downTime, downTime + 10, MotionEvent.ACTION_MOVE, startX - 400f, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(move, mViewParent));

        verify(mDelegate, never()).onSwipeStarted(anyBoolean());
    }

    @Test
    public void testSwipe_actionCancel() {
        when(mDelegate.onSwipeStarted(true)).thenReturn(mAdjacentView);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 300f;

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(down, mViewParent);
        mGestureHandler.onTouchEvent(down);

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(move, mViewParent);
        mGestureHandler.onTouchEvent(move);

        MotionEvent cancel =
                MotionEvent.obtain(
                        downTime, downTime + 20, MotionEvent.ACTION_CANCEL, endX, 500f, 0);
        mGestureHandler.onTouchEvent(cancel);

        verify(mDelegate).onSwipeCancelled();
    }

    @Test
    public void testOppositeFling_cancelsSwitch() {
        when(mDelegate.onSwipeStarted(true)).thenReturn(mAdjacentView);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 100f; // Large displacement (400px > 333px)

        doNothing().when(mVelocityTracker).computeCurrentVelocity(anyInt());
        // Flinging back to the right with high velocity while dragged left
        when(mVelocityTracker.getXVelocity()).thenReturn(2000f);
        when(mVelocityTracker.getYVelocity()).thenReturn(0f);

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(down, mViewParent);
        mGestureHandler.onTouchEvent(down);

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(move, mViewParent);
        mGestureHandler.onTouchEvent(move);

        MotionEvent up =
                MotionEvent.obtain(downTime, downTime + 20, MotionEvent.ACTION_UP, endX, 500f, 0);
        mGestureHandler.onTouchEvent(up);

        // Large displacement but opposite fling should cancel switch
        verify(mDelegate).onSwipeSettled(eq(false), eq(true));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE)
    public void testSwipe_disabledByFeatureFlag() {
        long downTime = SystemClock.uptimeMillis();
        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, 500f, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(down, mViewParent));
        assertFalse(mGestureHandler.onTouchEvent(down));

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, 100f, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(move, mViewParent));
        assertFalse(mGestureHandler.onTouchEvent(move));

        verify(mDelegate, never()).onSwipeStarted(anyBoolean());
    }

    @Test
    public void testSwipe_adjacentViewNull_bailsOutEarly() {
        when(mDelegate.onSwipeStarted(true)).thenReturn(null);

        long downTime = SystemClock.uptimeMillis();
        float startX = 500f;
        float endX = 100f;

        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, startX, 500f, 0);
        assertFalse(mGestureHandler.onInterceptTouchEvent(down, mViewParent));
        assertTrue(mGestureHandler.onTouchEvent(down));

        MotionEvent move1 =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, endX, 500f, 0);
        assertTrue(mGestureHandler.onInterceptTouchEvent(move1, mViewParent));
        // First MOVE past slop tries onSwipeStarted; when null, it bails out and returns false.
        assertFalse(mGestureHandler.onTouchEvent(move1));
        verify(mDelegate, times(1)).onSwipeStarted(true);

        // Subsequent MOVE events immediately bail out without calling onSwipeStarted again.
        MotionEvent move2 =
                MotionEvent.obtain(
                        downTime, downTime + 20, MotionEvent.ACTION_MOVE, endX - 50f, 500f, 0);
        assertFalse(mGestureHandler.onTouchEvent(move2));
        verify(mDelegate, times(1)).onSwipeStarted(true);

        // UP event also bails out without settling or cancelling.
        MotionEvent up =
                MotionEvent.obtain(
                        downTime, downTime + 30, MotionEvent.ACTION_UP, endX - 50f, 500f, 0);
        assertFalse(mGestureHandler.onTouchEvent(up));

        verify(mDelegate, never()).onSwipeSettled(anyBoolean(), anyBoolean());
        verify(mDelegate, never()).onSwipeCancelled();
    }

    @Test
    public void testOnInterceptTouchEvent_tracksMovementInVelocityTracker() {
        long downTime = SystemClock.uptimeMillis();
        MotionEvent down =
                MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, 500f, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(down, mViewParent);
        verify(mVelocityTracker).addMovement(down);

        MotionEvent move =
                MotionEvent.obtain(downTime, downTime + 10, MotionEvent.ACTION_MOVE, 490f, 500f, 0);
        mGestureHandler.onInterceptTouchEvent(move, mViewParent);
        verify(mVelocityTracker).addMovement(move);
    }
}
