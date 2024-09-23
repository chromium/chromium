// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabListOnScrollListener}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListOnScrollListenerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RecyclerView mRecyclerView;

    private TabListOnScrollListener mListener;

    @Before
    public void setUp() {
        mListener = new TabListOnScrollListener();
        doCallback(0, (Runnable r) -> r.run()).when(mRecyclerView).post(any());
    }

    @Test
    public void testPostUpdate() {
        assertNull(mListener.getYOffsetNonZeroSupplier().get());

        when(mRecyclerView.computeVerticalScrollOffset()).thenReturn(0);
        mListener.postUpdate(mRecyclerView);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        when(mRecyclerView.computeVerticalScrollOffset()).thenReturn(1);
        mListener.postUpdate(mRecyclerView);
        assertTrue(mListener.getYOffsetNonZeroSupplier().get());
    }

    @Test
    public void testOnScrolled() {
        when(mRecyclerView.computeVerticalScrollOffset()).thenReturn(1);
        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);

        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 0);
        assertNull(mListener.getYOffsetNonZeroSupplier().get());

        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_SETTLING);

        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 1);
        assertNull(mListener.getYOffsetNonZeroSupplier().get());

        when(mRecyclerView.computeVerticalScrollOffset()).thenReturn(0);
        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);
        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 0);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        when(mRecyclerView.computeVerticalScrollOffset()).thenReturn(3);
        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 2);
        assertTrue(mListener.getYOffsetNonZeroSupplier().get());

        when(mRecyclerView.computeVerticalScrollOffset()).thenReturn(-1);
        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 2);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());
    }
}
