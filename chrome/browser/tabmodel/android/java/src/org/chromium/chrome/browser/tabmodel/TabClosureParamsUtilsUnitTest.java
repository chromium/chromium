// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker.ListViewTouchInfo;

@RunWith(RobolectricTestRunner.class)
public class TabClosureParamsUtilsUnitTest {

    @Test
    public void shouldAllowUndo_nullListViewTouchTracker_returnTrue() {
        assertTrue(TabClosureParamsUtils.shouldAllowUndo(/* listViewTouchTracker= */ null));
    }

    @Test
    public void shouldAllowUndo_nullLastSingleTapUp_returnTrue() {
        FakeListViewTouchTracker fakeListViewTouchTracker = new FakeListViewTouchTracker();
        fakeListViewTouchTracker.setLastSingleTapUpInfo(null);

        assertTrue(TabClosureParamsUtils.shouldAllowUndo(fakeListViewTouchTracker));
    }

    @Test
    public void shouldAllowUndo_lastSingleTapUpFromTouchScreen_returnTrue() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker fakeListViewTouchTracker = new FakeListViewTouchTracker();
        fakeListViewTouchTracker.setLastSingleTapUpInfo(
                ListViewTouchInfo.fromMotionEvent(
                        createMotionEvent(
                                downMotionTime,
                                /* eventTime= */ downMotionTime + 50,
                                MotionEvent.ACTION_UP,
                                /* x= */ 0.0f,
                                /* y= */ 0.0f,
                                InputDevice.SOURCE_TOUCHSCREEN,
                                MotionEvent.TOOL_TYPE_FINGER)));

        assertTrue(TabClosureParamsUtils.shouldAllowUndo(fakeListViewTouchTracker));
    }

    @Test
    public void shouldAllowUndo_lastSingleTapUpFromMouse_returnFalse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker fakeListViewTouchTracker = new FakeListViewTouchTracker();
        fakeListViewTouchTracker.setLastSingleTapUpInfo(
                ListViewTouchInfo.fromMotionEvent(
                        createMotionEvent(
                                downMotionTime,
                                /* eventTime= */ downMotionTime + 50,
                                MotionEvent.ACTION_UP,
                                /* x= */ 0.0f,
                                /* y= */ 0.0f,
                                InputDevice.SOURCE_MOUSE,
                                MotionEvent.TOOL_TYPE_MOUSE)));

        assertFalse(TabClosureParamsUtils.shouldAllowUndo(fakeListViewTouchTracker));
    }

    /**
     * Creates a {@link MotionEvent}.
     *
     * <p>All parameters are for {@link MotionEvent#obtain}.
     */
    private static MotionEvent createMotionEvent(
            long downTime, long eventTime, int action, float x, float y, int source, int toolType) {
        PointerProperties pointerProperties = new MotionEvent.PointerProperties();
        pointerProperties.id = 0;
        pointerProperties.toolType = toolType;

        PointerCoords pointerCoords = new PointerCoords();
        pointerCoords.x = x;
        pointerCoords.y = y;

        return MotionEvent.obtain(
                downTime,
                eventTime,
                action,
                /* pointerCount= */ 1,
                new PointerProperties[] {pointerProperties},
                new PointerCoords[] {pointerCoords},
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 1.0f,
                /* yPrecision= */ 1.0f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                source,
                /* flags= */ 0);
    }
}
