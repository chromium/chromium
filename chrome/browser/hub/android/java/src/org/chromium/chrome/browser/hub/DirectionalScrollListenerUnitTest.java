// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link DirectionalScrollListener}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DirectionalScrollListenerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnScrollUp;
    @Mock private Runnable mOnScrollDown;
    @Mock private RecyclerView mRecyclerView;

    private DirectionalScrollListener mListener;

    @Before
    public void setUp() {
        mListener = new DirectionalScrollListener(mOnScrollUp, mOnScrollDown);
    }

    @Test
    @SmallTest
    public void testOnScrollDown() {
        mListener.onScrolled(mRecyclerView, 0, 20);
        verify(mOnScrollDown, times(1)).run();
        verify(mOnScrollUp, never()).run();
    }

    @Test
    @SmallTest
    public void testOnScrollUp() {
        mListener.onScrolled(mRecyclerView, 0, -20);
        verify(mOnScrollUp, times(1)).run();
        verify(mOnScrollDown, never()).run();
    }

    @Test
    @SmallTest
    public void testScrollThresholdNotMet() {
        mListener.onScrolled(mRecyclerView, 0, 1);
        verify(mOnScrollUp, never()).run();
        verify(mOnScrollDown, never()).run();

        mListener.onScrolled(mRecyclerView, 0, -1);
        verify(mOnScrollUp, never()).run();
        verify(mOnScrollDown, never()).run();
    }

    @Test
    @SmallTest
    public void testThrottling() {
        mListener.onScrolled(mRecyclerView, 0, 20);
        verify(mOnScrollDown, times(1)).run();
        verify(mOnScrollUp, never()).run();

        // Second scroll should be ignored due to throttling.
        mListener.onScrolled(mRecyclerView, 0, 20);
        verify(mOnScrollDown, times(1)).run();
        verify(mOnScrollUp, never()).run();

        // Wait for the throttle to expire.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Third scroll should trigger the callback again.
        mListener.onScrolled(mRecyclerView, 0, 20);
        verify(mOnScrollDown, times(2)).run();
        verify(mOnScrollUp, never()).run();
    }

    @Test
    @SmallTest
    public void testDifferentThresholds() {
        int scrollUpThreshold = 10;
        int scrollDownThreshold = 20;
        mListener =
                new DirectionalScrollListener(
                        mOnScrollUp, mOnScrollDown, 50, scrollUpThreshold, scrollDownThreshold);

        // Test scroll down threshold.
        mListener.onScrolled(mRecyclerView, 0, 15);
        verify(mOnScrollDown, never()).run();
        verify(mOnScrollUp, never()).run();

        mListener.onScrolled(mRecyclerView, 0, 25);
        verify(mOnScrollDown, times(1)).run();
        verify(mOnScrollUp, never()).run();

        // Wait for the throttle to expire.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Test scroll up threshold.
        mListener.onScrolled(mRecyclerView, 0, -5);
        verify(mOnScrollUp, never()).run();
        // Verify scroll down didn't run again.
        verify(mOnScrollDown, times(1)).run();

        mListener.onScrolled(mRecyclerView, 0, -15);
        verify(mOnScrollUp, times(1)).run();
        verify(mOnScrollDown, times(1)).run();
    }
}
