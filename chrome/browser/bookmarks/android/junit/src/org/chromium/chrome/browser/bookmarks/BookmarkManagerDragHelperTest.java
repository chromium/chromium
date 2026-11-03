// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BookmarkManagerDragHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkManagerDragHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock ItemTouchHelper mItemTouchHelper;
    @Mock View.OnClickListener mClickListener;

    private RecyclerView mRecyclerView;
    private FrameLayout mItemView;
    private View mDragHandle;
    private RecyclerView.ViewHolder mViewHolder;
    private Activity mActivity;
    private BookmarkManagerDragHelper mDragHelper;
    private BookmarkId mBookmarkId;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkId = new BookmarkId(12345L, BookmarkType.NORMAL);
        mRecyclerView = new RecyclerView(mActivity);
        mDragHandle = new View(mActivity);
        mDragHandle.setId(R.id.drag_handle);
        mItemView = new FrameLayout(mActivity);
        mItemView.addView(mDragHandle);
        mViewHolder = new RecyclerView.ViewHolder(mItemView) {};

        mDragHelper =
                new BookmarkManagerDragHelper(
                        mActivity,
                        mBookmarkId,
                        mSelectionDelegate,
                        mItemTouchHelper,
                        mRecyclerView,
                        mViewHolder,
                        /* isDragEnabled= */ true);
    }

    @Test
    public void testTouch_Scenario1_LongPressSelectsAndDrags() {
        // Scenario 1: Item is not selected.
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // Perform action down.
        boolean consumed =
                mDragHelper.onRowBodyTouch(
                        mItemView,
                        obtainEvent(
                                /* action= */ MotionEvent.ACTION_DOWN,
                                /* x= */ 50f,
                                /* y= */ 50f,
                                /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        assertTrue(consumed);

        // Checkpoint 1: Advance time to 499ms, just before selection.
        ShadowLooper.idleMainLooper(499, TimeUnit.MILLISECONDS);
        // Verify that the selection has not occurred yet.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());

        // Checkpoint 2: Advance time by another 100ms (Crossing the 500ms threshold).
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Selection should be triggered now.
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId);

        // Verify drag hasn't started yet here (599ms).
        verify(mItemTouchHelper, never()).startDrag(any());

        // Checkpoint 3: Advance another 100ms to fire the second runnable. Now at 699ms.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Dragging should have started.
        verify(mItemTouchHelper).startDrag(mViewHolder);
    }

    @Test
    public void testTouch_Scenario1_LongPressSelectsThenMoveImmediatelyStartsDrag() {
        // Scenario 1b: Item is not selected. User holds for 500ms (selection triggers), then
        // immediately moves finger during the 500ms-600ms window. Drag should start immediately
        // instead of being cancelled.
        doReturn(/* toBeReturned= */ false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        ShadowLooper.idleMainLooper(/* duration= */ 500, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId);
        verify(mItemTouchHelper, never()).startDrag(any());

        // Move finger past touch slop before the 100ms drag timer fires.
        boolean consumed =
                mDragHelper.onRowBodyTouch(
                        mItemView,
                        obtainEvent(
                                /* action= */ MotionEvent.ACTION_MOVE,
                                /* x= */ 100f,
                                /* y= */ 100f,
                                /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        assertTrue(consumed);
        verify(mItemTouchHelper).startDrag(mViewHolder);
    }

    @Test
    public void testTouch_Scenario2_AlreadySelectedHoldStartsDrag() {
        // Scenario 2: Item is already selected. A stationary hold starts drag after
        // SELECTION_START_DELAY_MS, without toggling selection.
        doReturn(/* toBeReturned= */ true).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // Perform action down.
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Verify a normal tap window does not prematurely start drag while stationary.
        ShadowLooper.idleMainLooper(/* duration= */ 499, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper, never()).startDrag(any());

        ShadowLooper.idleMainLooper(/* duration= */ 1, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper).startDrag(mViewHolder);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
    }

    @Test
    public void testTouch_Scenario2_AlreadySelectedMoveScrolls() {
        // Scenario 2b: Item is already selected. Moving past touch slop before the hold timer
        // fires is a scroll: the event is not consumed and no drag starts.
        doReturn(/* toBeReturned= */ true).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        boolean consumed =
                mDragHelper.onRowBodyTouch(
                        mItemView,
                        obtainEvent(
                                /* action= */ MotionEvent.ACTION_MOVE,
                                /* x= */ 100f,
                                /* y= */ 100f,
                                /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        assertFalse(consumed);

        ShadowLooper.idleMainLooper(/* duration= */ 1000, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper, never()).startDrag(any());
    }

    @Test
    public void testTouch_CanDragAgainWithoutRebind() {
        // Scenario: A row-body drag finishes without the row being rebound, so the same helper
        // handles the next gesture. The second long-press + move should still start a drag.
        doReturn(/* toBeReturned= */ false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        for (int i = 0; i < 2; i++) {
            mDragHelper.onRowBodyTouch(
                    mItemView,
                    obtainEvent(
                            /* action= */ MotionEvent.ACTION_DOWN,
                            /* x= */ 50f,
                            /* y= */ 50f,
                            /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
            ShadowLooper.idleMainLooper(/* duration= */ 500, TimeUnit.MILLISECONDS);
            mDragHelper.onRowBodyTouch(
                    mItemView,
                    obtainEvent(
                            /* action= */ MotionEvent.ACTION_MOVE,
                            /* x= */ 100f,
                            /* y= */ 100f,
                            /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
            // ItemTouchHelper takes over the gesture and cancels the row's touch stream.
            mDragHelper.onRowBodyTouch(
                    mItemView,
                    obtainEvent(
                            /* action= */ MotionEvent.ACTION_CANCEL,
                            /* x= */ 100f,
                            /* y= */ 100f,
                            /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        }

        verify(mItemTouchHelper, times(/* wantedNumberOfInvocations= */ 2)).startDrag(mViewHolder);
    }

    @Test
    public void testTouch_MovePastSlopThenUpDoesNotClick() {
        // Scenario: User moves past touch slop on an unselected row (e.g. a swipe the
        // RecyclerView did not intercept), then lifts. This must not be treated as a tap.
        doReturn(/* toBeReturned= */ false).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        mItemView.setOnClickListener(mClickListener);

        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_MOVE,
                        /* x= */ 100f,
                        /* y= */ 100f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_UP,
                        /* x= */ 100f,
                        /* y= */ 100f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        verify(mClickListener, never()).onClick(any());
    }

    @Test
    public void test_onViewDetachedFromWindow() {
        FrameLayout parent = new FrameLayout(mActivity);
        mActivity.setContentView(parent);
        parent.addView(mItemView);

        // Perform action down.
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Simulate the recyclerView recycling the view (ImprovedBookmarkRow gets detached).
        parent.removeView(mItemView);

        // Fast forward time significantly.
        ShadowLooper.idleMainLooper(10000, TimeUnit.MILLISECONDS);

        // Verify that the timer has died and that selection and drag have not happened.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());

        // Verify that the we cleaned up the listener: a subsequent detach no longer cancels the
        // timers.
        parent.addView(mItemView);
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        parent.removeView(mItemView);
        ShadowLooper.idleMainLooper(10000, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId);
    }

    @Test
    public void testDragHandle_DragWithFinger() {
        // Scenario: User touches the drag handle on a touchscreen.
        // Expectation: Drag starts immediately on ACTION_DOWN with zero delay. SOURCE_TOUCHSCREEN
        // also sets the SOURCE_CLASS_POINTER bit, so it must not be mistaken for a mouse.

        MotionEvent event =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        mDragHelper.onDragHandleTouch(mDragHandle, event);

        verify(mItemTouchHelper).startDrag(mViewHolder);
    }

    @Test
    public void testDragHandle_DragWithMouse() {
        // Scenario: User presses down on the grab handle with a mouse and drags.
        // Expectation: Drag starts immediately after exceeding touch slop.

        // 1. Mouse Down at (50, 50).
        MotionEvent downEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onDragHandleTouch(mDragHandle, downEvent);

        // 2. Verify that the drag has not started yet.
        verify(mItemTouchHelper, never()).startDrag(any());

        // 3. Mouse Move to (100, 100).
        // Distance is about 70px, which is > TouchSlop.
        MotionEvent moveEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_MOVE,
                        /* x= */ 100f,
                        /* y= */ 100f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onDragHandleTouch(mDragHandle, moveEvent);

        // 4. Verify start drag.
        verify(mItemTouchHelper).startDrag(mViewHolder);

        // 5. Verify the Row View (mItemView) gets the "closed hand" cursor.
        assertEquals(
                PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_GRABBING),
                mItemView.getPointerIcon());
    }

    @Test
    public void testDragHandle_MouseDownThenUp() {
        // Scenario: User clicks on the drag handle.
        // Expectation: Pointer icon changes to closed hand (GRABBING) and then to open hand (GRAB)
        // when mouse is up.

        // Item is selected.
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // 1. Mouse down.
        MotionEvent event =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onDragHandleTouch(mDragHandle, event);

        // Verify: Icon becomes "Closed Hand" (GRABBING).
        PointerIcon grabbingIcon = PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_GRABBING);
        assertEquals(grabbingIcon, mDragHandle.getPointerIcon());

        // 2. Mouse up.
        MotionEvent upEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_UP,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onDragHandleTouch(mDragHandle, upEvent);

        // Verify: Icon reverts to "Open Hand" (GRAB).
        PointerIcon grabIcon = PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_GRAB);
        assertEquals(grabIcon, mDragHandle.getPointerIcon());
    }

    @Test
    public void testHover_DragHandle_ShowsOpenHand() {
        // Scenario: Mouse hovers over the drag handle.
        // Expectation: Cursor changes from default to open hand.
        mDragHandle.setVisibility(View.GONE);

        MotionEvent hoverEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_MOVE,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onDragHandleHover(mDragHandle, hoverEvent);

        // Verify cursor set to Open Hand.
        PointerIcon openHand = PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_GRAB);
        assertEquals(openHand, mDragHandle.getPointerIcon());

        // Also verify it ensures the handle is visible (by calling onRowBodyHover logic
        // internally).
        assertEquals(View.VISIBLE, mDragHandle.getVisibility());
    }

    @Test
    public void testHover_UnselectedRowBody_ShowsAndHidesHandle() {
        // Scenario: Mouse enters row -> Handle is visible. Mouse exits -> Handle disappear after a
        // 50ms delay. This case only happens when the item is unselected.
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        mDragHandle.setVisibility(View.GONE);

        // 1. Mouse hover enter.
        MotionEvent enterEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onRowBodyHover(mItemView, enterEvent);

        // Verify handle becomes visible.
        assertEquals(View.VISIBLE, mDragHandle.getVisibility());

        // 2. Mouse hover exit.
        MotionEvent exitEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onRowBodyHover(mItemView, exitEvent);

        // Verify handle is not hidden immediately.
        assertEquals(View.VISIBLE, mDragHandle.getVisibility());

        // 3. Fast forward 50ms.
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);

        // Verify handle is now hidden.
        assertEquals(View.GONE, mDragHandle.getVisibility());
    }

    @Test
    public void testHover_SelectedRowBody_DoesNotHideHandle() {
        // Scenario: Item is selected. When the mouse exits the row, the handle should stay visible.
        doReturn(true).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // 1. Hover exit.
        MotionEvent exitEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onRowBodyHover(mItemView, exitEvent);

        // 2. Fast forward delay.
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);

        // Verify handle is still visible.
        assertEquals(View.VISIBLE, mDragHandle.getVisibility());
    }

    // Obtain the action event we want to perform (ACTION_DOWN, etc.).
    private MotionEvent obtainEvent(int action, float x, float y, int toolType) {
        // We get the current time since Android rejects times that are 0 or in the past.

        // When the finger/mouse first touches the screen.
        long downTime = SystemClock.uptimeMillis();
        // When the specific event happens (finger down, drag, lift finger, etc.).
        long eventTime = SystemClock.uptimeMillis();

        // Describes what (the mouse, finger, etc.) is touching the screen.
        MotionEvent.PointerProperties[] pointerProperties = new MotionEvent.PointerProperties[1];
        pointerProperties[0] = new MotionEvent.PointerProperties();
        pointerProperties[0].id = 0;
        pointerProperties[0].toolType = toolType;

        // Describes where the mouse/finger is touching the screen.
        MotionEvent.PointerCoords[] pointerCoords = new MotionEvent.PointerCoords[1];
        pointerCoords[0] = new MotionEvent.PointerCoords();
        pointerCoords[0].x = x;
        pointerCoords[0].y = y;

        // Manually create a fake event.
        return MotionEvent.obtain(
                /* downTime= */ downTime,
                /* eventTime= */ eventTime,
                /* action= */ action,
                /* pointerCount= */ 1,
                /* pointerProperties= */ pointerProperties,
                /* pointerCoords= */ pointerCoords,
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 1.0f,
                /* yPrecision= */ 1.0f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                /* source= */ 0,
                /* flags= */ 0);
    }
}
