// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;

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
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BookmarkManagerDragHelper}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ANDROID_BOOKMARK_BAR_FAST_FOLLOW)
public class BookmarkManagerDragHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock ItemTouchHelper mItemTouchHelper;
    @Mock RecyclerView mRecyclerView;
    @Mock View mItemView;

    @Mock View mDragHandle;

    RecyclerView.ViewHolder mViewHolder;
    private Activity mActivity;
    private BookmarkManagerDragHelper mDragHelper;
    private BookmarkId mBookmarkId;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkId = new BookmarkId(12345L, BookmarkType.NORMAL);
        mViewHolder = new RecyclerView.ViewHolder(mItemView) {};

        when(mItemView.getContext()).thenReturn(mActivity);
        doReturn(mDragHandle).when(mItemView).findViewById(R.id.drag_handle);

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
        // Scenario 1: None of the items in the list are selected.
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        doReturn(false).when(mSelectionDelegate).isSelectionEnabled();

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
    public void testTouch_Scenario2_AlreadySelectedStartsDragQuickly() {
        // Scenario 2: Selection Active, item is already selected.
        doReturn(true).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();

        // Perform action down.
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Verify drag starts quickly (100ms) without toggling selection.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper).startDrag(mViewHolder);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
    }

    @Test
    public void testTouch_Scenario3_SelectionActiveButNotThisItem() {
        // Scenario 3: Selection Active (other items are selected), but this item is not.
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // Perform action down.
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Verify that we long-press for 500ms. Drag should not have started at 499ms.
        ShadowLooper.idleMainLooper(499, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper, never()).startDrag(any());

        // Advance to 599ms.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Verify that drag has started.
        verify(mItemTouchHelper).startDrag(mViewHolder);
        // Verify that selection toggle has never happened.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
    }

    @Test
    public void test_onViewDetachedFromWindow() {
        // Perform action down.
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Simulate the recyclerView recycling the view (ImprovedBookmarkRow gets detached).
        mDragHelper.onViewDetachedFromWindow(mItemView);

        // Fast forward time significantly.
        ShadowLooper.idleMainLooper(10000, TimeUnit.MILLISECONDS);

        // Verify that the timer has died and that selection and drag have not happened.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());

        // Verify that the we cleaned up the listener.
        verify(mItemView).removeOnAttachStateChangeListener(mDragHelper);
    }

    @Test
    public void testDragHandle_DragWithFinger() {
        // Scenario: User touches the drag handle.
        // Expectation: It behaves like a normal semi-instant drag (100ms delay).

        // 1. Action Down.
        MotionEvent event =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER);
        mDragHelper.onDragHandleTouch(mDragHandle, event);

        // 2. Verify that the drag has not started immediately.
        verify(mItemTouchHelper, never()).startDrag(any());

        // 3. Fast forward 99ms. The drag should still not have started.
        ShadowLooper.idleMainLooper(99, TimeUnit.MILLISECONDS);
        verify(mItemTouchHelper, never()).startDrag(any());

        // 4. Fast forward an additional 100ms, now we are at 199s. The drag should have started at
        // the 100ms mark.
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
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
        verify(mItemView)
                .setPointerIcon(PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_GRABBING));

        // 5. Verify the synthetic event is dispatched to the Recycler View to ensuring the "closed
        // hand" cursor persists correctly during the drag.
        verify(mRecyclerView).dispatchTouchEvent(any(MotionEvent.class));
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
        verify(mDragHandle).setPointerIcon(grabbingIcon);

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
        verify(mDragHandle).setPointerIcon(grabIcon);
    }

    @Test
    public void testHover_DragHandle_ShowsOpenHand() {
        // Scenario: Mouse hovers over the drag handle.
        // Expectation: Cursor changes from default to open hand.

        MotionEvent hoverEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_MOVE,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onDragHandleHover(mDragHandle, hoverEvent);

        // Verify cursor set to Open Hand.
        PointerIcon openHand = PointerIcon.getSystemIcon(mActivity, PointerIcon.TYPE_GRAB);
        verify(mDragHandle).setPointerIcon(openHand);

        // Also verify it ensures the handle is visible (by calling onRowBodyHover logic
        // internally).
        verify(mDragHandle).setVisibility(View.VISIBLE);
    }

    @Test
    public void testHover_UnselectedRowBody_ShowsAndHidesHandle() {
        // Scenario: Mouse enters row -> Handle is visible. Mouse exits -> Handle disappear after a
        // 50ms delay. This case only happens when the item is unselected.
        doReturn(false).when(mSelectionDelegate).isItemSelected(mBookmarkId);

        // 1. Mouse hover enter.
        MotionEvent enterEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onRowBodyHover(mItemView, enterEvent);

        // Verify handle becomes visible.
        verify(mDragHandle).setVisibility(View.VISIBLE);

        // 2. Mouse hover exit.
        MotionEvent exitEvent =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mDragHelper.onRowBodyHover(mItemView, exitEvent);

        // Verify handle is not hidden immediately.
        verify(mDragHandle, never()).setVisibility(View.GONE);

        // 3. Fast forward 50ms.
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);

        // Verify handle is now hidden.
        verify(mDragHandle).setVisibility(View.GONE);
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
        verify(mDragHandle, never()).setVisibility(View.GONE);
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
