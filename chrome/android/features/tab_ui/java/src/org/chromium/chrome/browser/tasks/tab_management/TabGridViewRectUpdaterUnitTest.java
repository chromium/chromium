// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabGridViewRectUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGridViewRectUpdaterUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnRectChanged;

    private final View mView = new View(ContextUtils.getApplicationContext());
    private final FrameLayout mRootView = new FrameLayout(ContextUtils.getApplicationContext());
    private int mViewX;
    private int mViewY;
    private int mViewWidth;
    private int mViewHeight;
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

        mRootView.addView(mView);
        // Attach the root view as the root of its own window so that getLocationInWindow() works.
        Context context = ContextUtils.getApplicationContext();
        context.getSystemService(WindowManager.class)
                .addView(mRootView, new WindowManager.LayoutParams());
        ShadowLooper.idleMainLooper();
        mRootView.layout(0, 0, ROOT_VIEW_WIDTH, ROOT_VIEW_HEIGHT);

        // Scale around the top-left corner so that scaling does not change the view's location.
        mView.setPivotX(0);
        mView.setPivotY(0);
        mView.setScaleX(SCALE_X);
        mView.setScaleY(SCALE_Y);
        mViewX = INITIAL_X;
        mViewY = INITIAL_Y;
        setViewSize(VIEW_WIDTH, VIEW_HEIGHT);
    }

    private void setViewSize(int width, int height) {
        mViewWidth = width;
        mViewHeight = height;
        layoutView();
    }

    private void setViewLocation(int x, int y) {
        mViewX = x;
        mViewY = y;
        layoutView();
    }

    /**
     * Lays out the view manually. Tests do not idle the looper afterwards, so no layout traversal
     * overrides these bounds.
     */
    private void layoutView() {
        mView.layout(mViewX, mViewY, mViewX + mViewWidth, mViewY + mViewHeight);
    }

    @Test
    public void testRefreshRectBounds_firstCall() {
        mUpdater.refreshRectBounds(/* forceRefresh= */ false);

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
        setViewLocation(newX, newY);

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
        mView.setScaleX(newScaleX);
        mView.setScaleY(newScaleY);
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
        setViewSize(newViewWidth, newViewHeight);

        mUpdater.refreshRectBounds(/* forceRefresh= */ false);
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + newViewWidth, mRect.right);
        assertEquals(INITIAL_Y + newViewHeight, mRect.bottom);
        verify(mOnRectChanged, times(2)).run();
    }

    @Test
    public void testRefreshRectBounds_zeroScale() {
        mView.setScaleX(0f);
        mView.setScaleY(0f);

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
        setViewSize(ROOT_VIEW_WIDTH + 50, ROOT_VIEW_HEIGHT + 50);
        setViewLocation(0, 0);

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
        verify(mOnRectChanged, times(2)).run();
        assertEquals(INITIAL_X, mRect.left);
        assertEquals(INITIAL_Y, mRect.top);
        assertEquals(INITIAL_X + VIEW_WIDTH, mRect.right);
        assertEquals(INITIAL_Y + VIEW_HEIGHT, mRect.bottom);
    }
}
