// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.smoothScrollToPosition;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

/** Unit tests for {@link RecyclerViewScroller}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RecyclerViewScrollerUnitTest {
    private static final int FULL_HEIGHT = 100;
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

    private void mockTargetVisibility(boolean isVisible) {
        when(mRecyclerView.findViewHolderForAdapterPosition(0)).thenReturn(mTargetViewHolder);
        when(mTargetView.isShown()).thenReturn(isVisible);
        when(mTargetView.getMeasuredHeight()).thenReturn(FULL_HEIGHT);
        doCallback(
                        0,
                        item -> {
                            assert item instanceof Rect;
                            Rect rect = (Rect) item;
                            if (isVisible) {
                                rect.set(0, 0, 50, FULL_HEIGHT);
                            } else {
                                rect.set(0, 0, 50, FULL_HEIGHT - 1);
                            }
                        })
                .when(mTargetView)
                .getGlobalVisibleRect(any());
    }

    @Test
    public void testSmoothScrollToPosition_whenTargetIsVisible() {
        mockTargetVisibility(true);

        smoothScrollToPosition(mRecyclerView, 0, mOnScrollFinishedRunnable);

        verify(mOnScrollFinishedRunnable).run();
        verify(mLayoutManager, never()).startSmoothScroll(any());
    }

    @Test
    public void testSmoothScrollToPosition_whenTargetIsNotVisible() {
        mockTargetVisibility(false);

        smoothScrollToPosition(mRecyclerView, 0, mOnScrollFinishedRunnable);

        verify(mLayoutManager).startSmoothScroll(any());
    }
}
