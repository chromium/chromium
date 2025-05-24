// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.MotionEvent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.ui.util.MotionEventUtils;

@RunWith(RobolectricTestRunner.class)
public class TabClosureParamsUtilsUnitTest {

    @Test
    public void shouldAllowUndo_forListViewTouchTracker_nullListViewTouchTracker_returnTrue() {
        assertTrue(TabClosureParamsUtils.shouldAllowUndo((ListViewTouchTracker) null));
    }

    @Test
    public void shouldAllowUndo_forListViewTouchTracker_nullLastSingleTapUp_returnTrue() {
        FakeListViewTouchTracker fakeListViewTouchTracker = new FakeListViewTouchTracker();
        fakeListViewTouchTracker.setLastSingleTapUpInfo(null);

        assertTrue(TabClosureParamsUtils.shouldAllowUndo(fakeListViewTouchTracker));
    }

    @Test
    public void
            shouldAllowUndo_forListViewTouchTracker_lastSingleTapUpFromTouchScreen_returnTrue() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker fakeListViewTouchTracker = new FakeListViewTouchTracker();
        fakeListViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        assertTrue(TabClosureParamsUtils.shouldAllowUndo(fakeListViewTouchTracker));
    }

    @Test
    public void shouldAllowUndo_forListViewTouchTracker_lastSingleTapUpFromMouse_returnFalse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker fakeListViewTouchTracker = new FakeListViewTouchTracker();
        fakeListViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        assertFalse(TabClosureParamsUtils.shouldAllowUndo(fakeListViewTouchTracker));
    }

    @Test
    public void shouldAllowUndo_forTriggeringMotion_nullMotion_returnTrue() {
        assertTrue(TabClosureParamsUtils.shouldAllowUndo((MotionEventInfo) null));
    }

    @Test
    public void shouldAllowUndo_forTriggeringMotion_touchScreenMotion_returnTrue() {
        long downMotionTime = SystemClock.uptimeMillis();
        MotionEventInfo triggeringMotion =
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP);

        assertTrue(TabClosureParamsUtils.shouldAllowUndo(triggeringMotion));
    }

    @Test
    public void shouldAllowUndo_forTriggeringMotion_mouseMotion_returnFalse() {
        long downMotionTime = SystemClock.uptimeMillis();
        MotionEventInfo triggeringMotion =
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP);

        assertFalse(TabClosureParamsUtils.shouldAllowUndo(triggeringMotion));
    }

    @Test
    public void shouldAllowUndo_forDownMotionButtonState_buttonStateAbsent_returnTrue() {
        assertTrue(
                TabClosureParamsUtils.shouldAllowUndo(MotionEventUtils.MOTION_EVENT_BUTTON_NONE));
    }

    @Test
    public void shouldAllowUndo_forDownMotionButtonState_buttonStatePresent_returnFalse() {
        assertFalse(TabClosureParamsUtils.shouldAllowUndo(MotionEvent.BUTTON_PRIMARY));
    }
}
