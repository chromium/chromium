// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabRailHoverController.PointerState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link VerticalTabRailHoverController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabRailHoverControllerUnitTest {
    private static final int RAIL_WIDTH = 200;
    private static final int RAIL_HEIGHT = 500;
    private static final float INSIDE_X = 100f;
    // Within the collapsed rail width.
    private static final float COLLAPSED_PART_X = 40f;
    private static final float OUTSIDE_X = 500f;
    private static final float Y = 250f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabRailCollapseController mCollapseController;
    @Mock private WindowAndroid mWindowAndroid;

    private VerticalTabRailLayout mRailLayout;
    private VerticalTabRailHoverController mHoverController;
    private boolean mIsContextMenuShowing;

    @Before
    public void setUp() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        DeviceInfo.setIsDesktopForTesting(true);
        DeviceInput.setSupportsPrecisionPointerForTesting(true);
        when(mWindowAndroid.isTopResumedActivity()).thenReturn(true);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        Configuration config = activity.getResources().getConfiguration();
        config.smallestScreenWidthDp = 600;
        activity.getResources()
                .updateConfiguration(config, activity.getResources().getDisplayMetrics());

        mRailLayout =
                (VerticalTabRailLayout)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        mRailLayout.setCollapseState(RailCollapseState.COLLAPSED);
        mRailLayout.measure(
                View.MeasureSpec.makeMeasureSpec(RAIL_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(RAIL_HEIGHT, View.MeasureSpec.EXACTLY));
        mRailLayout.layout(0, 0, RAIL_WIDTH, RAIL_HEIGHT);

        mHoverController =
                new VerticalTabRailHoverController(
                        mRailLayout,
                        mCollapseController,
                        mWindowAndroid,
                        () -> mIsContextMenuShowing);
        verify(mWindowAndroid).addActivityStateObserver(mHoverController);
    }

    @After
    public void tearDown() {
        mHoverController.destroy();
        VerticalTabUtils.resetSharedPrefsForTesting();
    }

    @Test
    public void testHoverInsideRail_SetsHovering() {
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);

        verify(mCollapseController).setHovering(true);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverOverCollapseButton_DoesNotSetHovering() {
        View collapseButton = mRailLayout.getCollapseButton();
        int[] location = new int[2];
        collapseButton.getLocationOnScreen(location);

        dispatchMouseHover(
                MotionEvent.ACTION_HOVER_MOVE,
                location[0] + collapseButton.getWidth() / 2f,
                location[1] + collapseButton.getHeight() / 2f);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverDuringCollapseAnimation_DoesNotExpand() {
        // The collapse button was clicked: the rail animates to collapsed.
        when(mCollapseController.getEffectiveRailCollapseState())
                .thenReturn(RailCollapseState.COLLAPSED);
        mHoverController.setInTransition(true);

        // The pointer moves quickly towards the web contents across the shrinking rail.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, INSIDE_X + 1, Y);
        verify(mCollapseController, never()).setHovering(true);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());

        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);
        verify(mCollapseController, never()).setHovering(true);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverAfterCollapseAnimation_Expands() {
        when(mCollapseController.getEffectiveRailCollapseState())
                .thenReturn(RailCollapseState.COLLAPSED);
        mHoverController.setInTransition(true);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, COLLAPSED_PART_X, Y);
        verify(mCollapseController, never()).setHovering(true);

        // The collapse animation ends: the next hover over the rail expands it, e.g. when the
        // pointer moves down from the collapse button.
        mHoverController.setInTransition(false);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, COLLAPSED_PART_X, Y + 1);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);
        verify(mCollapseController).setHovering(true);
    }

    @Test
    public void testHoverOverCollapsedRail_ExpandsAfterDelay() {
        when(mCollapseController.getEffectiveRailCollapseState())
                .thenReturn(RailCollapseState.COLLAPSED);

        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, COLLAPSED_PART_X, Y);
        // Further moves do not restart the delay.
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS - 1);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, COLLAPSED_PART_X, Y + 1);
        verify(mCollapseController, never()).setHovering(true);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());

        idleMainLooper(1);
        verify(mCollapseController).setHovering(true);
    }

    @Test
    public void testQuickPassOverCollapsedRail_DoesNotExpand() {
        when(mCollapseController.getEffectiveRailCollapseState())
                .thenReturn(RailCollapseState.COLLAPSED);

        // The pointer moves from outside the window across the rail to the web contents.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, COLLAPSED_PART_X, Y);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(true);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverOverCollapsedRail_OntoCollapseButton_CancelsExpand() {
        when(mCollapseController.getEffectiveRailCollapseState())
                .thenReturn(RailCollapseState.COLLAPSED);
        View collapseButton = mRailLayout.getCollapseButton();
        int[] location = new int[2];
        collapseButton.getLocationOnScreen(location);

        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, COLLAPSED_PART_X, Y);
        dispatchMouseHover(
                MotionEvent.ACTION_HOVER_MOVE,
                location[0] + collapseButton.getWidth() / 2f,
                location[1] + collapseButton.getHeight() / 2f);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(true);
    }

    @Test
    public void testHoverDuringExpandAnimation_Expands() {
        // The rail animates to expanded for hovering, not to collapsed.
        when(mCollapseController.getEffectiveRailCollapseState())
                .thenReturn(RailCollapseState.EXPANDED_FOR_HOVERING);
        mHoverController.setInTransition(true);

        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, INSIDE_X, Y);

        verify(mCollapseController).setHovering(true);
    }

    @Test
    public void testHoverMoveOutsideRail_Ignored() {
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, OUTSIDE_X, Y);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverExitOutsideRail_StopsHovering() {
        hoverInsideRail();

        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);

        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverExitInsideRail_KeepsHovering() {
        hoverInsideRail();

        // A mouse button press or a covering window ends hover while the pointer is still inside.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverExitInsideRail_NoPressFollows_StopsHovering() {
        hoverInsideRail();

        // The pointer leaves the window straight from the rail: the HOVER_EXIT carries the last
        // position inside the rail, and no other event follows.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        verify(mCollapseController, never()).setHovering(anyBoolean());

        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS - 1);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        idleMainLooper(1);
        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverExitInsideRail_MousePressFollows_KeepsHovering() {
        hoverInsideRail();

        // A mouse button press sends a HOVER_EXIT, then a touch ACTION_DOWN.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        dispatchMouseTouch(MotionEvent.ACTION_DOWN, INSIDE_X, Y);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverExitInsideRail_HoverBackOverRail_KeepsHovering() {
        hoverInsideRail();

        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        clearInvocations(mCollapseController);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testHoverExitInsideRail_ContextMenuShowing_WaitsForDismissal() {
        hoverInsideRail();
        mIsContextMenuShowing = true;

        // The menu popup covers the pointer: its dismissal decides, not the exit check.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDestroy_CancelsHoverExitTimeout() {
        hoverInsideRail();
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);

        mHoverController.destroy();
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
    }

    @Test
    public void testNonMouseHover_Ignored() {
        MotionEvent event =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y, 0);
        event.setSource(InputDevice.SOURCE_STYLUS);
        mRailLayout.dispatchGenericMotionEvent(event);
        event.recycle();

        verify(mCollapseController, never()).setHovering(anyBoolean());
    }

    @Test
    public void testMouseRelease_OutsideRailStopsHovering() {
        hoverInsideRail();
        // Pressing a mouse button ends hover inside the rail.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);

        dispatchMouseTouch(MotionEvent.ACTION_UP, OUTSIDE_X, Y);

        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testMouseRelease_InsideRailIgnored() {
        hoverInsideRail();
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);

        dispatchMouseTouch(MotionEvent.ACTION_UP, INSIDE_X, Y);

        verify(mCollapseController, never()).setHovering(anyBoolean());
    }

    @Test
    public void testTouch_NonMouseAndNonReleaseIgnored() {
        hoverInsideRail();

        dispatchMouseTouch(MotionEvent.ACTION_DOWN, OUTSIDE_X, Y);
        MotionEvent fingerUp = MotionEvent.obtain(0, 0, MotionEvent.ACTION_UP, OUTSIDE_X, Y, 0);
        fingerUp.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        mRailLayout.dispatchTouchEvent(fingerUp);
        fingerUp.recycle();

        verify(mCollapseController, never()).setHovering(anyBoolean());
    }

    @Test
    public void testDrag_StopsHoveringWhenDragEnds() {
        hoverInsideRail();

        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());

        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);
        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDrag_IgnoredWhenPointerOutside() {
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDrag_LocationOverRail_KeepsHoveringWhenDragEnds() {
        hoverInsideRail();
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);

        // A drag location is only delivered to the rail while the pointer is over it.
        dispatchDrag(DragEvent.ACTION_DRAG_LOCATION);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());

        // E.g. ESC over the rail.
        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDrag_DropOnRail_KeepsHoveringWhenDragEnds() {
        hoverInsideRail();
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);

        dispatchDrag(DragEvent.ACTION_DROP);
        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDrag_ExitedRail_KeepsHoveringUntilDragEnds() {
        hoverInsideRail();
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchDrag(DragEvent.ACTION_DRAG_LOCATION);

        // The tab is dragged off the rail, e.g. to another window, without being released.
        dispatchDrag(DragEvent.ACTION_DRAG_EXITED);
        verify(mCollapseController, never()).setHovering(false);
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());

        // The tab is released away from the rail.
        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);
        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDrag_ExitedWhilePointerOutside_DoesNotStopHovering() {
        // E.g. a drag from another window passing over the rail.
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchDrag(DragEvent.ACTION_DRAG_EXITED);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDrag_LeavesAndReentersRailRepeatedly_KeepsHoveringUntilReleased() {
        hoverInsideRail();
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);

        // Out, back over the rail, and out again, without releasing.
        dispatchDrag(DragEvent.ACTION_DRAG_EXITED);
        dispatchDrag(DragEvent.ACTION_DRAG_LOCATION);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
        dispatchDrag(DragEvent.ACTION_DRAG_EXITED);
        verify(mCollapseController, never()).setHovering(anyBoolean());

        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);
        verify(mCollapseController).setHovering(false);
    }

    @Test
    public void testDrag_ReleasedOnRailAfterLeavingIt_KeepsHovering() {
        hoverInsideRail();
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchDrag(DragEvent.ACTION_DRAG_EXITED);

        dispatchDrag(DragEvent.ACTION_DRAG_LOCATION);
        dispatchDrag(DragEvent.ACTION_DROP);
        dispatchDrag(DragEvent.ACTION_DRAG_ENDED);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExternalDrag_OverRail_ExpandsAfterDebounceWhenNotTopResumed() {
        // The drag started in another window, which is the one in front.
        when(mWindowAndroid.isTopResumedActivity()).thenReturn(false);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_STARTED);

        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS - 1);
        verify(mCollapseController, never()).setHovering(anyBoolean());

        // Later LOCATION events do not postpone the expansion.
        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        idleMainLooper(1);
        verify(mCollapseController).setHovering(true);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExternalDrag_ExitsRail_StaysExpandedUntilDragEnds() {
        dragExternallyIntoRail();

        dispatchExternalDrag(DragEvent.ACTION_DRAG_EXITED);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());

        // The tab is dropped elsewhere.
        dispatchExternalDrag(DragEvent.ACTION_DRAG_ENDED);
        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExternalDrag_PassesAcrossRail_DoesNotExpand() {
        dispatchExternalDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);

        // The pointer leaves the rail before the debounce elapses.
        dispatchExternalDrag(DragEvent.ACTION_DRAG_EXITED);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(true);
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());

        // Coming back over the still collapsed rail expands it after the debounce.
        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);
        verify(mCollapseController).setHovering(true);
    }

    @Test
    public void testExternalDrag_ReentersRail_StaysExpanded() {
        dragExternallyIntoRail();
        dispatchExternalDrag(DragEvent.ACTION_DRAG_EXITED);

        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(false);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExternalDrag_DroppedOnRail_StaysExpanded() {
        dragExternallyIntoRail();

        dispatchExternalDrag(DragEvent.ACTION_DROP);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_ENDED);

        // The pointer is still over the rail: the hover events that follow take over.
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExternalDrag_EscOverRail_StaysExpanded() {
        dragExternallyIntoRail();
        dispatchExternalDrag(DragEvent.ACTION_DRAG_EXITED);

        // Back over the rail, then ESC: the drag ends without a drop.
        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_ENDED);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDragFromThisWindow_OverRail_DoesNotExpand() {
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchDrag(DragEvent.ACTION_DRAG_LOCATION);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExternalDrag_ExpandOnHoverDisabled_IgnoresEvents() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", false);

        dispatchExternalDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_EXITED);

        verifyNoInteractions(mCollapseController);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testContextMenu_DismissedWhileCoveringPointer_StopsHoveringAfterTimeout() {
        hoverInsideRail();
        mIsContextMenuShowing = true;

        // The menu popup covers the pointer, so hover ends while the pointer is inside the rail.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE_UNCONFIRMED, mHoverController.getPointerStateForTesting());

        // The menu is dismissed: hovering continues while waiting for a confirming hover event.
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();
        verify(mCollapseController, never()).setHovering(anyBoolean());

        // No hover event confirms the pointer over the rail in time: hovering stops once the
        // timeout elapses, and not before.
        idleMainLooper(VerticalTabRailHoverController.MENU_DISMISS_TIMEOUT_MS - 1);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        idleMainLooper(1);
        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testContextMenu_DismissedWhileCoveringPointer_HoverInTimeKeepsHovering() {
        hoverInsideRail();
        mIsContextMenuShowing = true;
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();

        // Once the popup is gone, a hover event confirms the pointer is still over the rail.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        idleMainLooper(VerticalTabRailHoverController.MENU_DISMISS_TIMEOUT_MS);

        verify(mCollapseController, never()).setHovering(false);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testContextMenu_DismissedWithPointerSeenInsideKeepsHovering() {
        hoverInsideRail();
        mIsContextMenuShowing = true;
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);

        // The pointer is seen over an uncovered part of the rail before the menu is dismissed.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, INSIDE_X, Y);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();
        idleMainLooper(VerticalTabRailHoverController.MENU_DISMISS_TIMEOUT_MS);

        verify(mCollapseController, never()).setHovering(false);
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testContextMenu_DismissedThenHoverOverCollapseButton_KeepsHovering() {
        hoverInsideRail();
        mIsContextMenuShowing = true;
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();

        // Once the popup is gone, a hover event confirms the pointer is over the collapse button.
        View collapseButton = mRailLayout.getCollapseButton();
        int[] location = new int[2];
        collapseButton.getLocationOnScreen(location);
        dispatchMouseHover(
                MotionEvent.ACTION_HOVER_ENTER,
                location[0] + collapseButton.getWidth() / 2f,
                location[1] + collapseButton.getHeight() / 2f);
        idleMainLooper(VerticalTabRailHoverController.MENU_DISMISS_TIMEOUT_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testContextMenu_DismissedWhileCoveringPointer_HoverExitStopsHoveringOnce() {
        hoverInsideRail();
        mIsContextMenuShowing = true;
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();

        // The pointer is seen leaving the rail: hovering stops right away.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);
        verify(mCollapseController).setHovering(false);

        // The confirmation wait is cancelled, so hovering is not stopped again.
        idleMainLooper(VerticalTabRailHoverController.MENU_DISMISS_TIMEOUT_MS);
        verify(mCollapseController, times(1)).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testDestroy_CancelsMenuDismissTimeout() {
        hoverInsideRail();
        mIsContextMenuShowing = true;
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, INSIDE_X, Y);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();

        mHoverController.destroy();
        idleMainLooper(VerticalTabRailHoverController.MENU_DISMISS_TIMEOUT_MS);

        verify(mCollapseController, never()).setHovering(anyBoolean());
    }

    @Test
    public void testTopResumedLost_StopsHovering() {
        hoverInsideRail();

        when(mWindowAndroid.isTopResumedActivity()).thenReturn(false);
        mHoverController.onActivityTopResumedChanged(false);

        verify(mCollapseController).setHovering(false);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testTopResumedLost_WhileContextMenuShowing_DefersUntilDismissed() {
        hoverInsideRail();
        mIsContextMenuShowing = true;

        // Another activity comes to the front while the menu is showing: hovering continues.
        when(mWindowAndroid.isTopResumedActivity()).thenReturn(false);
        mHoverController.onActivityTopResumedChanged(false);
        verify(mCollapseController, never()).setHovering(anyBoolean());

        // Dismissing the menu stops hovering.
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();
        verify(mCollapseController).setHovering(false);
    }

    @Test
    public void testHoverWhileNotTopResumed_ExpandsOnlyAfterTopResumed() {
        when(mWindowAndroid.isTopResumedActivity()).thenReturn(false);

        // Hover events reach a window that is not in front, but the rail does not expand.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.INSIDE, mHoverController.getPointerStateForTesting());

        // The activity becomes top resumed: the next hover event expands the rail.
        when(mWindowAndroid.isTopResumedActivity()).thenReturn(true);
        mHoverController.onActivityTopResumedChanged(true);
        verify(mCollapseController, never()).setHovering(anyBoolean());
        dispatchMouseHover(MotionEvent.ACTION_HOVER_MOVE, INSIDE_X, Y);
        verify(mCollapseController).setHovering(true);
    }

    @Test
    public void testContextMenu_DefersHoverCollapseUntilDismissed() {
        hoverInsideRail();
        mIsContextMenuShowing = true;

        // The pointer leaves the rail while the menu is showing: hovering continues.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);
        verify(mCollapseController, never()).setHovering(anyBoolean());

        // Dismissing the menu stops hovering.
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();
        verify(mCollapseController).setHovering(false);
    }

    @Test
    public void testContextMenu_HoverEnterCancelsPendingCollapse() {
        hoverInsideRail();
        mIsContextMenuShowing = true;

        // The pointer leaves the rail and comes back before the menu is dismissed.
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();

        verify(mCollapseController, never()).setHovering(false);
    }

    @Test
    public void testExpandOnHoverDisabled_IgnoresEvents() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", false);

        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);
        dispatchMouseTouch(MotionEvent.ACTION_UP, OUTSIDE_X, Y);
        dispatchDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchDrag(DragEvent.ACTION_DRAG_EXITED);
        mHoverController.onActivityTopResumedChanged(false);

        verifyNoInteractions(mCollapseController);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExpandOnHoverTurnedOffByUser_IgnoresEvents() {
        VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                false, ExpandOnHoverToggleEntryPoint.SETTINGS);
        clearInvocations(mCollapseController);

        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        dispatchMouseHover(MotionEvent.ACTION_HOVER_EXIT, OUTSIDE_X, Y);

        verify(mCollapseController, never()).setHovering(true);
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());
    }

    @Test
    public void testExpandOnHoverTurnedOffByUser_StopsHovering() {
        hoverInsideRail();
        // Turned off from a context menu, which is still showing.
        mIsContextMenuShowing = true;

        VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                false, ExpandOnHoverToggleEntryPoint.TAB_STRIP_CONTEXT_MENU);

        // The rail collapses right away instead of waiting for the menu to be dismissed.
        verify(mCollapseController).onExpandOnHoverSettingChanged();
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());

        // Nothing is pending once the menu is dismissed.
        clearInvocations(mCollapseController);
        mIsContextMenuShowing = false;
        mHoverController.onContextMenuDismissed();
        verify(mCollapseController, never()).setHovering(anyBoolean());
    }

    @Test
    public void testExpandOnHoverTurnedOnByUser_NextHoverExpands() {
        VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                false, ExpandOnHoverToggleEntryPoint.SETTINGS);
        clearInvocations(mCollapseController);

        // Turning it on syncs Side UI, as the setting changes whether the rail can be resized.
        VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                true, ExpandOnHoverToggleEntryPoint.SETTINGS);
        verify(mCollapseController).onExpandOnHoverSettingChanged();
        verify(mCollapseController, never()).setHovering(anyBoolean());
        assertEquals(PointerState.OUTSIDE, mHoverController.getPointerStateForTesting());

        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        verify(mCollapseController).setHovering(true);
    }

    @Test
    public void testDestroy_StopsObservingRail() {
        mHoverController.destroy();
        verify(mWindowAndroid).removeActivityStateObserver(mHoverController);

        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        VerticalTabUtils.setExpandOnHoverEnabledInSharedPref(
                false, ExpandOnHoverToggleEntryPoint.SETTINGS);

        verifyNoInteractions(mCollapseController);
    }

    /** Hovers the rail, then forgets the resulting interaction with the collapse controller. */
    private void hoverInsideRail() {
        dispatchMouseHover(MotionEvent.ACTION_HOVER_ENTER, INSIDE_X, Y);
        verify(mCollapseController).setHovering(true);
        clearInvocations(mCollapseController);
    }

    /**
     * Drags a tab from another window over the rail until it expands, then forgets the resulting
     * interaction with the collapse controller.
     */
    private void dragExternallyIntoRail() {
        dispatchExternalDrag(DragEvent.ACTION_DRAG_STARTED);
        dispatchExternalDrag(DragEvent.ACTION_DRAG_LOCATION);
        idleMainLooper(VerticalTabRailHoverController.HOVER_DEBOUNCE_MS);
        verify(mCollapseController).setHovering(true);
        clearInvocations(mCollapseController);
    }

    /** Feeds the hover controller a drag event of a drag started in this window. */
    private void dispatchDrag(int action) {
        mHoverController.onDragEvent(mockDragEvent(action), /* isExternalDrag= */ false);
    }

    /** Feeds the hover controller a drag event of a drag started in another window. */
    private void dispatchExternalDrag(int action) {
        mHoverController.onDragEvent(mockDragEvent(action), /* isExternalDrag= */ true);
    }

    private void dispatchMouseHover(int action, float x, float y) {
        MotionEvent event = MotionEvent.obtain(0, 0, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_MOUSE);
        mRailLayout.dispatchGenericMotionEvent(event);
        event.recycle();
    }

    private void dispatchMouseTouch(int action, float x, float y) {
        MotionEvent event = MotionEvent.obtain(0, 0, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_MOUSE);
        mRailLayout.dispatchTouchEvent(event);
        event.recycle();
    }

    private static void idleMainLooper(long delayMs) {
        ShadowLooper.idleMainLooper(delayMs, TimeUnit.MILLISECONDS);
    }

    private static DragEvent mockDragEvent(int action) {
        DragEvent event = mock(DragEvent.class);
        when(event.getAction()).thenReturn(action);
        return event;
    }
}
