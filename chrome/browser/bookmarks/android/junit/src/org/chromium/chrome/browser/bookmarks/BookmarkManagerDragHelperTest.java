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
import android.view.View.MeasureSpec;
import android.view.ViewGroup;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;
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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BookmarkManagerDragHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkManagerDragHelperTest {
    private static class TestRecyclerView extends RecyclerView {
        final List<Boolean> mDisallowInterceptCalls = new ArrayList<>();

        TestRecyclerView(Activity activity) {
            super(activity);
        }

        @Override
        public void requestDisallowInterceptTouchEvent(boolean disallowIntercept) {
            mDisallowInterceptCalls.add(disallowIntercept);
            super.requestDisallowInterceptTouchEvent(disallowIntercept);
        }
    }

    private class SingleHolderAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private int mItemCount = 1;

        void removeItemWithoutLayout() {
            mItemCount = 0;
            notifyItemRemoved(0);
        }

        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return mViewHolder;
        }

        @Override
        public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return mItemCount;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock ItemTouchHelper mItemTouchHelper;
    @Mock View.OnClickListener mClickListener;

    private TestRecyclerView mRecyclerView;
    private SingleHolderAdapter mAdapter;
    private ImprovedBookmarkRow mItemView;
    private View mDragHandle;
    private RecyclerView.ViewHolder mViewHolder;
    private Activity mActivity;
    private BookmarkManagerDragHelper mDragHelper;
    private BookmarkId mBookmarkId;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mBookmarkId = new BookmarkId(/* id= */ 12345L, BookmarkType.NORMAL);
        mRecyclerView = new TestRecyclerView(mActivity);
        mDragHandle = new View(mActivity);
        mDragHandle.setId(R.id.drag_handle);
        mItemView = new ImprovedBookmarkRow(mActivity, /* attrs= */ null);
        mItemView.addView(mDragHandle);
        mViewHolder = new RecyclerView.ViewHolder(mItemView) {};

        mAdapter = new SingleHolderAdapter();
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mActivity));
        mRecyclerView.setAdapter(mAdapter);
        mActivity.setContentView(mRecyclerView);
        int spec = MeasureSpec.makeMeasureSpec(/* size= */ 500, MeasureSpec.EXACTLY);
        mRecyclerView.measure(spec, spec);
        mRecyclerView.layout(/* l= */ 0, /* t= */ 0, /* r= */ 500, /* b= */ 500);
        mRecyclerView.mDisallowInterceptCalls.clear();

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
        assertTrue(mItemView.isAttachedToWindow());
        ViewGroup parent = (ViewGroup) mRecyclerView.getParent();

        // Perform action down (before 500ms, disallowIntercept has not been requested).
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Detach the hierarchy so ImprovedBookmarkRow receives onViewDetachedFromWindow.
        parent.removeView(mRecyclerView);
        assertFalse(mItemView.isAttachedToWindow());

        // Detaching without having requested disallow-intercept must not clear another row's
        // disallow-intercept state.
        assertTrue(mRecyclerView.mDisallowInterceptCalls.isEmpty());

        // Fast forward time significantly.
        ShadowLooper.idleMainLooper(/* duration= */ 10000, TimeUnit.MILLISECONDS);

        // Verify that the timer has died and that selection and drag have not happened.
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());

        // Verify that after a transient detach/re-attach, the helper stays active and subsequent
        // detaches still cancel pending timers.
        parent.addView(mRecyclerView);
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        parent.removeView(mRecyclerView);
        ShadowLooper.idleMainLooper(/* duration= */ 600, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());

        // Re-attaching still allows normal long-press selection while attached.
        parent.addView(mRecyclerView);
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        ShadowLooper.idleMainLooper(/* duration= */ 500, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId);

        // Explicit destroy() is idempotent and disables touch/hover handlers.
        mDragHelper.destroy();
        mItemView.setPressed(/* pressed= */ true);
        mDragHelper.destroy();
        assertTrue(mItemView.isPressed());

        assertFalse(
                mDragHelper.onRowBodyTouch(
                        mItemView,
                        obtainEvent(
                                /* action= */ MotionEvent.ACTION_DOWN,
                                /* x= */ 50f,
                                /* y= */ 50f,
                                /* toolType= */ MotionEvent.TOOL_TYPE_FINGER)));
        MotionEvent handleDown =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER);
        handleDown.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        assertFalse(mDragHelper.onDragHandleTouch(mDragHandle, handleDown));
        mDragHandle.setVisibility(View.GONE);
        MotionEvent hoverEnter =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        assertFalse(mDragHelper.onRowBodyHover(mItemView, hoverEnter));
        assertFalse(mDragHelper.onDragHandleHover(mDragHandle, hoverEnter));
        assertEquals(View.GONE, mDragHandle.getVisibility());
        verify(mItemTouchHelper, never()).startDrag(any());
    }

    @Test
    public void test_onViewDetachedFromWindow_AfterSelection_DoesNotStartDragOnMove() {
        doReturn(/* toBeReturned= */ false).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        ViewGroup parent = (ViewGroup) mRecyclerView.getParent();

        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Hold for 500ms so selection triggers, disallow-intercept is requested, and
        // mIsLongPressTriggered is set.
        ShadowLooper.idleMainLooper(/* duration= */ 500, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId);
        assertEquals(List.of(true), mRecyclerView.mDisallowInterceptCalls);

        // Detach the view (e.g. search filters out the row) before the finger moves; destroy()
        // resets disallow-intercept because this helper requested it.
        parent.removeView(mRecyclerView);
        assertEquals(List.of(true, false), mRecyclerView.mDisallowInterceptCalls);

        // Subsequent ACTION_MOVE on the in-flight gesture stream must not start a drag.
        boolean consumed =
                mDragHelper.onRowBodyTouch(
                        mItemView,
                        obtainEvent(
                                /* action= */ MotionEvent.ACTION_MOVE,
                                /* x= */ 100f,
                                /* y= */ 100f,
                                /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        assertFalse(consumed);
        verify(mItemTouchHelper, never()).startDrag(any());
    }

    @Test
    public void test_selectAndStartDrag_SkipWhenDetachedFromAdapterOrAdapterNull() {
        doReturn(/* toBeReturned= */ false).when(mSelectionDelegate).isItemSelected(mBookmarkId);
        assertTrue(mItemView.isAttachedToWindow());
        assertEquals(0, mViewHolder.getBindingAdapterPosition());

        // Remove the item from the adapter without running layout yet so mItemView stays attached
        // to the window while getBindingAdapterPosition() becomes NO_POSITION.
        mAdapter.removeItemWithoutLayout();
        assertTrue(mItemView.isAttachedToWindow());
        assertEquals(RecyclerView.NO_POSITION, mViewHolder.getBindingAdapterPosition());

        // 1. selectItem() (500ms timer) skips when bindingAdapterPosition == NO_POSITION.
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        ShadowLooper.idleMainLooper(/* duration= */ 600, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());

        // 2. startDrag() (immediate finger handle touch) skips when bindingAdapterPosition ==
        // NO_POSITION.
        MotionEvent handleDown =
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER);
        handleDown.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        mDragHelper.onDragHandleTouch(mDragHandle, handleDown);
        verify(mItemTouchHelper, never()).startDrag(any());

        // 3. Clearing the adapter (getAdapter() == null) also treats the ViewHolder as detached.
        mRecyclerView.setAdapter(/* adapter= */ null);
        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        ShadowLooper.idleMainLooper(/* duration= */ 600, TimeUnit.MILLISECONDS);
        mDragHelper.onDragHandleTouch(mDragHandle, handleDown);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());
    }

    @Test
    public void testImprovedBookmarkRow_SetDragHelperDestroysPreviousHelperOnRebind() {
        assertTrue(mItemView.isAttachedToWindow());
        assertEquals(0, mViewHolder.getBindingAdapterPosition());
        ViewGroup parent = (ViewGroup) mRecyclerView.getParent();
        mItemView.setDragHelper(mDragHelper);

        // Simulate a transient detach/re-attach before the row is rebound; mItemView must still
        // track mDragHelper so setDragHelper(secondHelper) destroys it.
        parent.removeView(mRecyclerView);
        parent.addView(mRecyclerView);

        mDragHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));

        // Rebinding mItemView in-place to a new BookmarkManagerDragHelper must destroy mDragHelper
        // and cancel its pending timers even though mViewHolder stays attached and bound.
        BookmarkId reboundBookmarkId = new BookmarkId(/* id= */ 67890L, BookmarkType.NORMAL);
        BookmarkManagerDragHelper secondHelper =
                new BookmarkManagerDragHelper(
                        mActivity,
                        reboundBookmarkId,
                        mSelectionDelegate,
                        mItemTouchHelper,
                        mRecyclerView,
                        mViewHolder,
                        /* isDragEnabled= */ true);
        mItemView.setDragHelper(secondHelper);

        ShadowLooper.idleMainLooper(/* duration= */ 600, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate, never()).toggleSelectionForItem(any());
        verify(mItemTouchHelper, never()).startDrag(any());

        // Positive control: the newly bound helper still selects its rebound BookmarkId.
        secondHelper.onRowBodyTouch(
                mItemView,
                obtainEvent(
                        /* action= */ MotionEvent.ACTION_DOWN,
                        /* x= */ 50f,
                        /* y= */ 50f,
                        /* toolType= */ MotionEvent.TOOL_TYPE_FINGER));
        ShadowLooper.idleMainLooper(/* duration= */ 500, TimeUnit.MILLISECONDS);
        verify(mSelectionDelegate).toggleSelectionForItem(reboundBookmarkId);
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
