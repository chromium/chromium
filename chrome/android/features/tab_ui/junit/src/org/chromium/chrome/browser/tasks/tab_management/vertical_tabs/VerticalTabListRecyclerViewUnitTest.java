// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView.Adapter;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;

/** Unit tests for {@link VerticalTabListRecyclerView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabListRecyclerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final int DEFAULT_SCROLL_OFFSET = 12345;
    private static final int DEFAULT_SCROLL_RANGE = 54321;
    private static final int TOTAL_ITEM_COUNT = 10;

    @Mock private LinearLayoutManager mLayoutManager;
    @Mock private Adapter<?> mAdapter;

    private VerticalTabListRecyclerView mRecyclerView;
    private View mFirstView;
    private View mLastView;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mRecyclerView = new VerticalTabListRecyclerView(context, null);
        mFirstView = new View(context);
        mLastView = new View(context);

        when(mLayoutManager.canScrollVertically()).thenReturn(true);
        when(mLayoutManager.computeVerticalScrollOffset(any())).thenReturn(DEFAULT_SCROLL_OFFSET);
        when(mLayoutManager.computeVerticalScrollRange(any())).thenReturn(DEFAULT_SCROLL_RANGE);
        mRecyclerView.setLayoutManager(mLayoutManager);

        when(mAdapter.getItemCount()).thenReturn(TOTAL_ITEM_COUNT);
        when(mAdapter.getItemViewType(0)).thenReturn(TabProperties.UiType.PINNED_TAB);
        when(mAdapter.getItemViewType(1)).thenReturn(TabProperties.UiType.PINNED_TAB);
        for (int i = 2; i < TOTAL_ITEM_COUNT; i++) {
            when(mAdapter.getItemViewType(i)).thenReturn(TabProperties.UiType.TAB);
        }
        mRecyclerView.setAdapter(mAdapter);
    }

    @Test
    public void testComputeVerticalScroll_NoPinnedTabs_DelegatesToSuper() {
        for (int i = 0; i < TOTAL_ITEM_COUNT; i++) {
            when(mAdapter.getItemViewType(i)).thenReturn(TabProperties.UiType.TAB);
        }

        assertEquals(DEFAULT_SCROLL_OFFSET, mRecyclerView.computeVerticalScrollOffset());
        assertEquals(DEFAULT_SCROLL_RANGE, mRecyclerView.computeVerticalScrollRange());
        verify(mLayoutManager).computeVerticalScrollOffset(any());
        verify(mLayoutManager).computeVerticalScrollRange(any());
    }

    @Test
    public void testComputeVerticalScrollOffset_WithPinnedPlaceholders() {
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2);
        when(mLayoutManager.findLastVisibleItemPosition()).thenReturn(4);
        when(mLayoutManager.findViewByPosition(2)).thenReturn(mFirstView);
        when(mLayoutManager.findViewByPosition(4)).thenReturn(mLastView);

        mFirstView.layout(0, /* t= */ 0, 100, /* b= */ 100);
        mLastView.layout(0, /* t= */ 200, 100, /* b= */ 300);

        assertEquals(0, mRecyclerView.computeVerticalScrollOffset());
        verify(mLayoutManager, never()).computeVerticalScrollOffset(any());
    }

    @Test
    public void testComputeVerticalScrollOffset_ScrolledDown() {
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(4);
        when(mLayoutManager.findLastVisibleItemPosition()).thenReturn(7);
        when(mLayoutManager.findViewByPosition(4)).thenReturn(mFirstView);
        when(mLayoutManager.findViewByPosition(7)).thenReturn(mLastView);

        mFirstView.layout(0, /* t= */ 0, 100, /* b= */ 100);
        mLastView.layout(0, /* t= */ 300, 100, /* b= */ 400);

        assertEquals(200, mRecyclerView.computeVerticalScrollOffset());
        verify(mLayoutManager, never()).computeVerticalScrollOffset(any());
    }

    @Test
    public void testComputeVerticalScrollRange_FitsCompletely_HidesScrollbar() {
        when(mAdapter.getItemCount()).thenReturn(4);
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2);
        when(mLayoutManager.findLastVisibleItemPosition()).thenReturn(3);
        when(mLayoutManager.findViewByPosition(2)).thenReturn(mFirstView);
        when(mLayoutManager.findViewByPosition(3)).thenReturn(mLastView);

        mRecyclerView.setBottom(500);
        mFirstView.layout(0, /* t= */ 0, 100, /* b= */ 100);
        mLastView.layout(0, /* t= */ 100, 100, /* b= */ 200);

        assertEquals(
                mRecyclerView.computeVerticalScrollExtent(),
                mRecyclerView.computeVerticalScrollRange());
        verify(mLayoutManager, never()).computeVerticalScrollRange(any());
    }

    @Test
    public void testComputeVerticalScrollRange_ContentOverflows_ShowsScrollbar() {
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2);
        when(mLayoutManager.findLastVisibleItemPosition()).thenReturn(5);
        when(mLayoutManager.findViewByPosition(2)).thenReturn(mFirstView);
        when(mLayoutManager.findViewByPosition(5)).thenReturn(mLastView);

        mRecyclerView.setBottom(300);
        mFirstView.layout(0, /* t= */ 0, 100, /* b= */ 100);
        mLastView.layout(0, /* t= */ 300, 100, /* b= */ 400);

        assertEquals(800, mRecyclerView.computeVerticalScrollRange());
        verify(mLayoutManager, never()).computeVerticalScrollRange(any());
    }

    @Test
    public void testOnInterceptHoverEvent_ReturnsFalse() {
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_MOVE, 0, 0, 0);
        assertFalse(mRecyclerView.onInterceptHoverEvent(event));
    }
}
