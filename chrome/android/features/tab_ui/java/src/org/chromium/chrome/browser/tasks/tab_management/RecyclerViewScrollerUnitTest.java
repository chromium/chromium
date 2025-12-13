// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentCaptor.captor;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.isScrollingUp;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.isTargetFullyVisible;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.smoothScrollToPosition;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

/** Unit tests for {@link RecyclerViewScroller}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RecyclerViewScrollerUnitTest {
    private static final int FULL_HEIGHT = 100;
    private static final int TARGET_INDEX = 0;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListRecyclerView mRecyclerView;
    @Mock private LinearLayoutManager mLayoutManager;
    @Mock private Runnable mOnScrollFinishedRunnable;
    @Mock private View mTargetView;

    private ViewHolder mTargetViewHolder;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        when(mRecyclerView.getContext()).thenReturn(context);
        when(mRecyclerView.getLayoutManager()).thenReturn(mLayoutManager);

        mTargetViewHolder = new ViewHolder(mTargetView, (a, b, c) -> {});
    }

    @Test
    public void testSmoothScrollToPosition_whenTargetIsFullyVisible() {
        mockTargetFullyVisible(TARGET_INDEX);

        smoothScrollToPosition(mRecyclerView, TARGET_INDEX, mOnScrollFinishedRunnable);

        verify(mOnScrollFinishedRunnable).run();
        verify(mLayoutManager, never()).startSmoothScroll(any());
    }

    @Test
    public void testSmoothScrollToPosition_whenTargetIsNotFullyVisible() {
        mockTargetPartiallyVisible(TARGET_INDEX);

        smoothScrollToPosition(mRecyclerView, TARGET_INDEX, mOnScrollFinishedRunnable);

        verify(mLayoutManager).startSmoothScroll(any());
    }

    @Test
    public void testSmoothScrollToPosition_nullLayoutManager() {
        when(mRecyclerView.getLayoutManager()).thenReturn(null);
        smoothScrollToPosition(mRecyclerView, TARGET_INDEX, mOnScrollFinishedRunnable);
        verify(mOnScrollFinishedRunnable).run();
        verify(mRecyclerView, never()).addOnScrollListener(any());
    }

    @Test
    public void testOnScrollFinished_whenScrollCompletes() {
        mockTargetPartiallyVisible(TARGET_INDEX);
        ArgumentCaptor<OnScrollListener> listenerCaptor = captor();

        smoothScrollToPosition(mRecyclerView, TARGET_INDEX, mOnScrollFinishedRunnable);

        verify(mRecyclerView).addOnScrollListener(listenerCaptor.capture());
        OnScrollListener listener = listenerCaptor.getValue();

        // Simulate auto-scroll completion.
        listener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_IDLE);

        verify(mOnScrollFinishedRunnable).run();
        verify(mRecyclerView).removeOnScrollListener(listener);
    }

    @Test
    public void testOnScrollFinished_whenScrollInterrupted() {
        mockTargetPartiallyVisible(TARGET_INDEX);
        ArgumentCaptor<OnScrollListener> listenerCaptor = captor();

        smoothScrollToPosition(mRecyclerView, TARGET_INDEX, mOnScrollFinishedRunnable);

        verify(mRecyclerView).addOnScrollListener(listenerCaptor.capture());
        OnScrollListener listener = listenerCaptor.getValue();

        // Simulate auto-scroll being interrupted by user.
        listener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_SETTLING);
        verify(mOnScrollFinishedRunnable, never()).run();
        listener.onScrollStateChanged(mRecyclerView, RecyclerView.SCROLL_STATE_DRAGGING);

        verify(mOnScrollFinishedRunnable).run();
        verify(mRecyclerView).removeOnScrollListener(listener);
    }

    @Test
    public void testIsScrollingUp_noChildren() {
        when(mLayoutManager.getChildCount()).thenReturn(0);
        assertFalse(isScrollingUp(0, mLayoutManager));
    }

    @Test
    public void testIsScrollingUp_scrollingUp() {
        when(mLayoutManager.getChildCount()).thenReturn(1);
        View child = mock();
        when(mLayoutManager.getChildAt(0)).thenReturn(child);
        when(mLayoutManager.getPosition(child)).thenReturn(5);

        assertTrue(isScrollingUp(0, mLayoutManager));
    }

    @Test
    public void testIsScrollingUp_scrollingDown() {
        when(mLayoutManager.getChildCount()).thenReturn(1);
        View child = mock();
        when(mLayoutManager.getChildAt(0)).thenReturn(child);
        when(mLayoutManager.getPosition(child)).thenReturn(0);

        assertFalse(isScrollingUp(5, mLayoutManager));
    }

    @Test
    public void testIsTargetFullyVisible_viewHolderNull() {
        when(mRecyclerView.findViewHolderForAdapterPosition(TARGET_INDEX)).thenReturn(null);
        assertFalse(isTargetFullyVisible(mRecyclerView, TARGET_INDEX));
    }

    @Test
    public void testIsTargetFullyVisible_notShown() {
        when(mRecyclerView.findViewHolderForAdapterPosition(TARGET_INDEX))
                .thenReturn(mTargetViewHolder);
        when(mTargetView.isShown()).thenReturn(false);
        assertFalse(isTargetFullyVisible(mRecyclerView, TARGET_INDEX));
    }

    @Test
    public void testIsTargetFullyVisible_partiallyVisible() {
        mockTargetPartiallyVisible(TARGET_INDEX);
        assertFalse(isTargetFullyVisible(mRecyclerView, TARGET_INDEX));
    }

    @Test
    public void testIsTargetFullyVisible_fullyVisible() {
        mockTargetFullyVisible(TARGET_INDEX);
        assertTrue(isTargetFullyVisible(mRecyclerView, TARGET_INDEX));
    }

    private void mockTargetFullyVisible(int targetIndex) {
        when(mRecyclerView.findViewHolderForAdapterPosition(targetIndex))
                .thenReturn(mTargetViewHolder);
        when(mTargetView.isShown()).thenReturn(true);
        when(mTargetView.getMeasuredHeight()).thenReturn(FULL_HEIGHT);
        doCallback(
                        0,
                        (Rect rect) -> {
                            rect.set(0, 0, 50, FULL_HEIGHT);
                        })
                .when(mTargetView)
                .getGlobalVisibleRect(any());
    }

    private void mockTargetPartiallyVisible(int targetIndex) {
        when(mRecyclerView.findViewHolderForAdapterPosition(targetIndex))
                .thenReturn(mTargetViewHolder);
        when(mTargetView.isShown()).thenReturn(true);
        when(mTargetView.getMeasuredHeight()).thenReturn(FULL_HEIGHT);
        doCallback(
                        0,
                        (Rect rect) -> {
                            rect.set(0, 0, 50, FULL_HEIGHT - 1);
                        })
                .when(mTargetView)
                .getGlobalVisibleRect(any());
    }
}
