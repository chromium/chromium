// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabGridViewRectUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGridViewRectUpdaterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mView;
    @Mock private View mRootView;
    @Mock private Runnable mOnRectChanged;

    private Rect mRect;
    private TabGridViewRectUpdater mUpdater;

    private static final int INITIAL_X = 10;
    private static final int INITIAL_Y = 20;
    private static final int VIEW_WIDTH = 100;
    private static final int VIEW_HEIGHT = 200;
    private static final float SCALE_X = 1.0f;
    private static final float SCALE_Y = 1.0f;
    private static final int ROOT_VIEW_WIDTH = 800;
    private static final int ROOT_VIEW_HEIGHT = 600;

    @Before
    public void setUp() {
        mRect = new Rect();
        mUpdater = new TabGridViewRectUpdater(mView, mRect, mOnRectChanged);

        // Default mock setup
        when(mView.getWidth()).thenReturn(VIEW_WIDTH);
        when(mView.getHeight()).thenReturn(VIEW_HEIGHT);
        when(mView.getScaleX()).thenReturn(SCALE_X);
        when(mView.getScaleY()).thenReturn(SCALE_Y);
        when(mView.getRootView()).thenReturn(mRootView);
        when(mRootView.getWidth()).thenReturn(ROOT_VIEW_WIDTH);
        when(mRootView.getHeight()).thenReturn(ROOT_VIEW_HEIGHT);

        doAnswer(
                        invocation -> {
                            int[] coords = invocation.getArgument(0);
                            coords[0] = INITIAL_X;
                            coords[1] = INITIAL_Y;
                            return null;
                        })
                .when(mView)
                .getLocationInWindow(any());
    }

    @Test
    public void testRefreshRectBounds_firstCall() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);

        verify(mView).getLocationInWindow(any());
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + VIEW_WIDTH, mRect.right);
        assertEquals(INITIAL_Y + VIEW_HEIGHT, mRect.bottom);
        verify(mOnRectChanged, times(1)).run();
    }

    @Test
    public void testRefreshRectBounds_noChange() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        verify(mOnRectChanged, times(1)).run();

        // Second call doesn't do anything since the rect hasn't changed.
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        verify(mView, times(2)).getLocationInWindow(any());
        verify(mOnRectChanged, times(1)).run();
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + VIEW_WIDTH, mRect.right);
        assertEquals(INITIAL_Y + VIEW_HEIGHT, mRect.bottom);
    }

    @Test
    public void testRefreshRectBounds_locationChanged_updatesRectAndNotifies() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        verify(mOnRectChanged, times(1)).run();

        final int newX = 50;
        final int newY = 60;
        doAnswer(
                        invocation -> {
                            int[] coords = invocation.getArgument(0);
                            coords[0] = newX;
                            coords[1] = newY;
                            return null;
                        })
                .when(mView)
                .getLocationInWindow(any());

        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        assertEquals(newX, mRect.left);
        assertEquals(newY, mRect.top);
        assertEquals(newX + VIEW_WIDTH, mRect.right);
        assertEquals(newY + VIEW_HEIGHT, mRect.bottom);
        verify(mOnRectChanged, times(2)).run();
    }

    @Test
    public void testRefreshRectBounds_scaleChanged() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        verify(mOnRectChanged, times(1)).run();

        float newScaleX = 0.5f;
        float newScaleY = 0.8f;
        when(mView.getScaleX()).thenReturn(newScaleX);
        when(mView.getScaleY()).thenReturn(newScaleY);
        int expectedScaledWidth = (int) (VIEW_WIDTH * newScaleX);
        int expectedScaledHeight = (int) (VIEW_HEIGHT * newScaleY);

        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + expectedScaledWidth, mRect.right);
        assertEquals(INITIAL_Y + expectedScaledHeight, mRect.bottom);
        verify(mOnRectChanged, times(2)).run();
    }

    @Test
    public void testRefreshRectBounds_viewDimensionsChanged() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        verify(mOnRectChanged, times(1)).run();

        int newViewWidth = 150;
        int newViewHeight = 250;
        when(mView.getWidth()).thenReturn(newViewWidth);
        when(mView.getHeight()).thenReturn(newViewHeight);

        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + newViewWidth, mRect.right);
        assertEquals(INITIAL_Y + newViewHeight, mRect.bottom);
        verify(mOnRectChanged, times(2)).run();
    }

    @Test
    public void testRefreshRectBounds_zeroScale() {
        when(mView.getScaleX()).thenReturn(0f);
        when(mView.getScaleY()).thenReturn(0f);

        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(mRect.left, mRect.right);
        assertEquals(mRect.top, mRect.bottom);
        verify(mOnRectChanged, times(1)).run();
    }

    @Test
    public void testRefreshRectBounds_exceedsRootViewSize() {
        // Make the view itself larger than the root view.
        when(mView.getWidth()).thenReturn(ROOT_VIEW_WIDTH + 50);
        when(mView.getHeight()).thenReturn(ROOT_VIEW_HEIGHT + 50);
        doAnswer(
                        invocation -> {
                            int[] coords = invocation.getArgument(0);
                            coords[0] = 0;
                            coords[1] = 0;
                            return null;
                        })
                .when(mView)
                .getLocationInWindow(any());

        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        assertEquals(0, mRect.left);
        assertEquals(0, mRect.top);
        assertEquals(ROOT_VIEW_WIDTH, mRect.right);
        assertEquals(ROOT_VIEW_HEIGHT, mRect.bottom);
        verify(mOnRectChanged, times(1)).run();
    }

    @Test
    public void testRefreshRectBounds_forceRefresh() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        verify(mOnRectChanged, times(1)).run();

        mUpdater.refreshRectBounds(/* forceRefresh= */ true);
        verify(mView, times(2)).getLocationInWindow(any());

        verify(mOnRectChanged, times(2)).run();
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + VIEW_WIDTH, mRect.right);
        assertEquals(INITIAL_Y + VIEW_HEIGHT, mRect.bottom);
    }
}
