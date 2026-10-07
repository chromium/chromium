// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.isScrollingUp;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.isTargetFullyVisible;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.smoothScrollToPosition;

import android.app.Activity;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

/** Unit tests for {@link RecyclerViewScroller}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RecyclerViewScrollerUnitTest {
    private static class TestAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        @Override
        public @NonNull RecyclerView.ViewHolder onCreateViewHolder(
                @NonNull ViewGroup parent, int viewType) {
            View view = new View(parent.getContext());
            view.setLayoutParams(
                    new RecyclerView.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, ITEM_HEIGHT_PX));
            return new RecyclerView.ViewHolder(view) {};
        }

        @Override
        public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {}

        @Override
        public int getItemCount() {
            return ITEM_COUNT;
        }
    }

    private static final int ITEM_COUNT = 30;
    private static final int ITEM_HEIGHT_PX = 100;
    // Items 0 and 1 are fully visible, item 2 is partially visible.
    private static final int RECYCLER_VIEW_HEIGHT_PX = 250;
    private static final int FULLY_VISIBLE_INDEX = 0;
    private static final int PARTIALLY_VISIBLE_INDEX = 2;
    // Far enough away that smooth scrolling to it takes several frames.
    private static final int FAR_AWAY_INDEX = ITEM_COUNT - 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnScrollFinishedRunnable;

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private LinearLayoutManager mLayoutManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRecyclerView = new RecyclerView(mActivity);
        mLayoutManager = new LinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.setAdapter(new TestAdapter());
        mActivity.setContentView(
                mRecyclerView,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, RECYCLER_VIEW_HEIGHT_PX));
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    public void testSmoothScrollToPosition_whenTargetIsFullyVisible() {
        smoothScrollToPosition(mRecyclerView, FULLY_VISIBLE_INDEX, mOnScrollFinishedRunnable);

        verify(mOnScrollFinishedRunnable).run();
        assertFalse(mLayoutManager.isSmoothScrolling());
    }

    @Test
    public void testSmoothScrollToPosition_whenTargetIsNotFullyVisible() {
        smoothScrollToPosition(mRecyclerView, PARTIALLY_VISIBLE_INDEX, mOnScrollFinishedRunnable);

        assertTrue(mLayoutManager.isSmoothScrolling());
        verify(mOnScrollFinishedRunnable, never()).run();
    }

    @Test
    public void testSmoothScrollToPosition_nullLayoutManager() {
        mRecyclerView.setLayoutManager(null);
        smoothScrollToPosition(mRecyclerView, FULLY_VISIBLE_INDEX, mOnScrollFinishedRunnable);
        verify(mOnScrollFinishedRunnable).run();
    }

    @Test
    public void testOnScrollFinished_whenScrollCompletes() {
        smoothScrollToPosition(mRecyclerView, FAR_AWAY_INDEX, mOnScrollFinishedRunnable);
        verify(mOnScrollFinishedRunnable, never()).run();

        // Let the auto-scroll run to completion.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        assertEquals(RecyclerView.SCROLL_STATE_IDLE, mRecyclerView.getScrollState());
        assertTrue(isTargetFullyVisible(mRecyclerView, FAR_AWAY_INDEX));
        verify(mOnScrollFinishedRunnable).run();

        // The listener should have been removed, so further scrolls do not trigger it again.
        mRecyclerView.smoothScrollBy(0, -ITEM_HEIGHT_PX);
        mRecyclerView.stopScroll();
        verify(mOnScrollFinishedRunnable).run();
    }

    @Test
    public void testOnScrollFinished_whenScrollInterrupted() {
        smoothScrollToPosition(mRecyclerView, FAR_AWAY_INDEX, mOnScrollFinishedRunnable);
        assertTrue(mLayoutManager.isSmoothScrolling());
        verify(mOnScrollFinishedRunnable, never()).run();

        // Simulate the auto-scroll being interrupted by the user dragging the list.
        int touchSlop = ViewConfiguration.get(mActivity).getScaledTouchSlop();
        long now = SystemClock.uptimeMillis();
        MotionEvent down =
                MotionEvent.obtain(
                        now,
                        now,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 10,
                        /* y= */ 200,
                        /* metaState= */ 0);
        MotionEvent move =
                MotionEvent.obtain(
                        now,
                        now + 10,
                        MotionEvent.ACTION_MOVE,
                        /* x= */ 10,
                        /* y= */ 200 - 2 * touchSlop,
                        /* metaState= */ 0);
        mRecyclerView.dispatchTouchEvent(down);
        mRecyclerView.dispatchTouchEvent(move);
        down.recycle();
        move.recycle();
        assertEquals(RecyclerView.SCROLL_STATE_DRAGGING, mRecyclerView.getScrollState());
        assertFalse(mLayoutManager.isSmoothScrolling());
        verify(mOnScrollFinishedRunnable).run();

        // The listener should have been removed, so further state changes do not trigger it.
        mRecyclerView.stopScroll();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mOnScrollFinishedRunnable).run();
    }

    @Test
    public void testIsScrollingUp() {
        // No children.
        assertFalse(isScrollingUp(0, new LinearLayoutManager(mActivity)));

        // Scrolling down.
        assertFalse(isScrollingUp(5, mLayoutManager));

        // Scrolling up.
        mLayoutManager.scrollToPositionWithOffset(5, 0);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(isScrollingUp(0, mLayoutManager));
    }

    @Test
    public void testIsTargetFullyVisible() {
        // No ViewHolder for the target.
        assertFalse(isTargetFullyVisible(mRecyclerView, FAR_AWAY_INDEX));
        assertFalse(isTargetFullyVisible(mRecyclerView, PARTIALLY_VISIBLE_INDEX));
        assertTrue(isTargetFullyVisible(mRecyclerView, FULLY_VISIBLE_INDEX));

        // Not shown.
        mRecyclerView.setVisibility(View.INVISIBLE);
        assertFalse(isTargetFullyVisible(mRecyclerView, FULLY_VISIBLE_INDEX));
    }
}
