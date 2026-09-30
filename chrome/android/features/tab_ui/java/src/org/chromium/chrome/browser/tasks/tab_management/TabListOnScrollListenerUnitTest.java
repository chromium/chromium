// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabListOnScrollListener}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListOnScrollListenerUnitTest {
    /** A LayoutManager with a canned vertical scroll offset. */
    private static class TestLayoutManager extends LinearLayoutManager {
        public int mVerticalScrollOffset;

        TestLayoutManager(Context context) {
            super(context);
        }

        @Override
        public int computeVerticalScrollOffset(RecyclerView.State state) {
            return mVerticalScrollOffset;
        }
    }

    private RecyclerView mRecyclerView;
    private TestLayoutManager mLayoutManager;

    private TabListOnScrollListener mListener;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mLayoutManager = new TestLayoutManager(activity);
        mRecyclerView = new RecyclerView(activity);
        mRecyclerView.setLayoutManager(mLayoutManager);
        // Attach the RecyclerView so that posted tasks run.
        activity.setContentView(mRecyclerView);
        ShadowLooper.idleMainLooper();

        mListener = new TabListOnScrollListener();
    }

    /**
     * Starts a smooth scroll to enter the settling state. It stays settling as long as the looper
     * is not idled.
     */
    private void setScrollStateSettling() {
        mRecyclerView.smoothScrollBy(0, 100);
        assertEquals(RecyclerView.SCROLL_STATE_SETTLING, mRecyclerView.getScrollState());
    }

    private void setScrollStateIdle() {
        mRecyclerView.stopScroll();
        assertEquals(RecyclerView.SCROLL_STATE_IDLE, mRecyclerView.getScrollState());
    }

    @Test
    public void testPostUpdate() {
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        mLayoutManager.mVerticalScrollOffset = 0;
        mListener.postUpdate(mRecyclerView);
        ShadowLooper.idleMainLooper();
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        mLayoutManager.mVerticalScrollOffset = 1;
        mListener.postUpdate(mRecyclerView);
        ShadowLooper.idleMainLooper();
        assertTrue(mListener.getYOffsetNonZeroSupplier().get());
    }

    @Test
    public void testOnScrolled() {
        mLayoutManager.mVerticalScrollOffset = 1;
        setScrollStateIdle();

        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 0);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        setScrollStateSettling();

        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 1);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        mLayoutManager.mVerticalScrollOffset = 0;
        setScrollStateIdle();
        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 0);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());

        mLayoutManager.mVerticalScrollOffset = 3;
        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 2);
        assertTrue(mListener.getYOffsetNonZeroSupplier().get());

        mLayoutManager.mVerticalScrollOffset = -1;
        mListener.onScrolled(mRecyclerView, /* dx= */ 0, /* dy= */ 2);
        assertFalse(mListener.getYOffsetNonZeroSupplier().get());
    }
}
