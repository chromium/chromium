// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.RectF;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AreaMotionEventFilter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AreaMotionEventFilterUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private MotionEventHandler mHandler;

    private AreaMotionEventFilter mEventFilter;
    private RectF mTriggerRect;
    private MotionEvent mHoverEnterEvent;
    private MotionEvent mHoverMoveEvent;
    private MotionEvent mHoverExitEvent;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        context.getResources().getDisplayMetrics().density = 1.0f;
        mTriggerRect = new RectF(0, 0, 100, 100);
        mEventFilter = new AreaMotionEventFilter(context, mHandler, mTriggerRect, false, false);

        mHoverEnterEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 1.f, 1.f, 0);
        mHoverMoveEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_MOVE, 10.f, 10.f, 0);
        mHoverExitEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 101.f, 101.f, 0);
    }

    @Test
    public void testHoverEnterExitInterceptionInFilterArea() {
        // Intercept an ACTION_HOVER_ENTER into the filter area.
        boolean intercepted = mEventFilter.onInterceptHoverEvent(mHoverEnterEvent);
        Assert.assertTrue("Hover enter event into area should be intercepted.", intercepted);
        Assert.assertTrue(
                "|mHasHoverEnterOrMoveEventInArea| should be set to true.",
                mEventFilter.getHasHoverEnterOrMoveEventInAreaForTesting());

        // Intercept an ACTION_HOVER_EXIT from the filter area.
        intercepted = mEventFilter.onInterceptHoverEvent(mHoverExitEvent);
        Assert.assertTrue("Hover exit event from area should be intercepted.", intercepted);
        Assert.assertFalse(
                "|mHasHoverEnterOrMoveEventInArea| should be set to false.",
                mEventFilter.getHasHoverEnterOrMoveEventInAreaForTesting());
        Assert.assertTrue(
                "|mHoverExitedArea| should be set to true.",
                mEventFilter.getHoverExitedAreaForTesting());
    }

    @Test
    public void testHoverExitInterceptionWithinFilterArea() {
        // Intercept an ACTION_HOVER_ENTER into the filter area.
        boolean intercepted = mEventFilter.onInterceptHoverEvent(mHoverEnterEvent);

        // Intercept an ACTION_HOVER_EXIT inside the filter area potentially triggered by another
        // gesture motion event. In this case the hover exit action will be recorded from within the
        // rect.
        var hoverExitEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 50.f, 50.f, 0);
        intercepted = mEventFilter.onInterceptHoverEvent(hoverExitEvent);
        Assert.assertTrue("Hover exit event in area should be intercepted.", intercepted);
        Assert.assertFalse(
                "|mHasHoverEnterOrMoveEventInArea| should be set to false.",
                mEventFilter.getHasHoverEnterOrMoveEventInAreaForTesting());
    }

    @Test
    public void testHoverEnterInterceptionOutsideFilterArea() {
        // Do not intercept an ACTION_HOVER_ENTER outside the filter area.
        mHoverEnterEvent.setLocation(101.f, 101.f);
        boolean intercepted = mEventFilter.onInterceptHoverEvent(mHoverEnterEvent);
        Assert.assertFalse(
                "Hover enter event outside area should not be intercepted.", intercepted);
    }

    @Test
    public void testHoverEnterInFilterArea() {
        // Intercept an ACTION_HOVER_ENTER into the filter area.
        mEventFilter.onInterceptHoverEvent(mHoverEnterEvent);

        // Handle the hover enter event.
        mEventFilter.onHoverEvent(mHoverEnterEvent);
        verify(mHandler).onHoverEnter(mHoverEnterEvent.getX(), mHoverEnterEvent.getY());
    }

    @Test
    public void testHoverMoveInFilterArea() {
        // Intercept an ACTION_HOVER_MOVE in the filter area.
        mEventFilter.onInterceptHoverEvent(mHoverMoveEvent);

        // Handle the hover move event.
        mEventFilter.onHoverEvent(mHoverMoveEvent);
        verify(mHandler).onHoverMove(mHoverMoveEvent.getX(), mHoverMoveEvent.getY());
    }

    @Test
    public void testHoverExitFromFilterArea_OnActionExit() {
        // Simulate an ACTION_HOVER_ENTER into the filter area.
        mEventFilter.onInterceptHoverEvent(mHoverEnterEvent);
        mEventFilter.onHoverEvent(mHoverEnterEvent);

        // Intercept a subsequent ACTION_HOVER_EXIT from the filter area.
        mEventFilter.onInterceptHoverEvent(mHoverExitEvent);

        // Handle the hover exit event.
        mEventFilter.onHoverEvent(mHoverExitEvent);
        verify(mHandler).onHoverExit();
    }

    @Test
    public void testHoverExitFromFilterArea_OnActionMove() {
        // Simulate an ACTION_HOVER_ENTER into the filter area.
        mEventFilter.onInterceptHoverEvent(mHoverEnterEvent);
        mEventFilter.onHoverEvent(mHoverEnterEvent);

        // Intercept a subsequent ACTION_HOVER_MOVE outside the filter area.
        mHoverMoveEvent.setLocation(101.f, 101.f);
        mEventFilter.onInterceptHoverEvent(mHoverMoveEvent);

        // Handle the hover move event.
        mEventFilter.onHoverEvent(mHoverMoveEvent);
        // Moving outside the filter area will be considered an ACTION_HOVER_EXIT for further
        // handling.
        verify(mHandler).onHoverExit();
    }

    @Test
    public void testGenericMotionEvent_interceptActionGeneratingEvents() {
        verifyGenericMotionEvent(
                MotionEvent.ACTION_BUTTON_RELEASE,
                MotionEvent.TOOL_TYPE_MOUSE,
                InputDevice.SOURCE_CLASS_POINTER);
        verifyGenericMotionEvent(
                MotionEvent.ACTION_BUTTON_PRESS,
                MotionEvent.TOOL_TYPE_MOUSE,
                InputDevice.SOURCE_CLASS_POINTER);

        verifyGenericMotionEvent(
                MotionEvent.ACTION_BUTTON_RELEASE,
                MotionEvent.TOOL_TYPE_FINGER,
                InputDevice.SOURCE_MOUSE);
        verifyGenericMotionEvent(
                MotionEvent.ACTION_BUTTON_PRESS,
                MotionEvent.TOOL_TYPE_FINGER,
                InputDevice.SOURCE_MOUSE);

        verify(mHandler, never()).onScroll(anyFloat(), anyFloat());
    }

    @Test
    public void testGenericMotionEvent_handleMouseScroll() {
        verifyGenericMotionEvent(
                MotionEvent.ACTION_SCROLL,
                MotionEvent.TOOL_TYPE_MOUSE,
                InputDevice.SOURCE_CLASS_POINTER);
        verify(mHandler).onScroll(anyFloat(), anyFloat());
    }

    @Test
    public void testGenericMotionEvent_handleTrackpadScroll() {
        verifyGenericMotionEvent(
                MotionEvent.ACTION_SCROLL, MotionEvent.TOOL_TYPE_FINGER, InputDevice.SOURCE_MOUSE);
        verify(mHandler).onScroll(anyFloat(), anyFloat());
    }

    private void verifyGenericMotionEvent(int action, int toolType, int source) {
        Assert.assertTrue(
                mEventFilter.onGenericMotionEvent(
                        createGenericMotionEvent(1f, 1f, action, toolType, source)));

        Assert.assertFalse(
                mEventFilter.onGenericMotionEvent(
                        createGenericMotionEvent(
                                mTriggerRect.width() + 1,
                                mTriggerRect.height() + 1,
                                action,
                                toolType,
                                source)));
    }

    private static MotionEvent createGenericMotionEvent(
            float x, float y, int action, int toolType, int source) {
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = x;
        coords[0].y = y;

        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = toolType;

        return MotionEvent.obtain(0, 0, action, 1, properties, coords, 0, 0, x, y, 0, 0, source, 0);
    }
}
