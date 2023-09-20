// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabImageView}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
public class TabImageViewUnitTest {
    private Activity mActivity;
    private FrameLayout mRootView;

    private TabImageView mTabImageView;
    @Mock
    private Runnable mRunnable;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ActivityController<Activity> activityController = Robolectric.buildActivity(Activity.class);
        activityController.setup();
        mActivity = activityController.get();

        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(mRootView);

        mTabImageView = new TabImageView(mActivity);
    }

    @Test
    @SmallTest
    public void testRunsImmediatelyWhenDetached() {
        assertFalse(mTabImageView.isAttachedToWindow());

        mTabImageView.setOnNextLayoutRunnable(mRunnable);
        verify(mRunnable, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsImmediatelyIfNotWaitingForLayout() {
        mRootView.addView(mTabImageView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mTabImageView.isAttachedToWindow());
        assertFalse(mTabImageView.isLayoutRequested());

        mTabImageView.setOnNextLayoutRunnable(mRunnable);
        verify(mRunnable, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsOnNextLayout() {
        mRootView.addView(mTabImageView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        ShadowLooper.runUiThreadTasks();
        assertTrue(mTabImageView.isAttachedToWindow());

        mTabImageView.requestLayout();
        assertTrue(mTabImageView.isLayoutRequested());
        mTabImageView.setOnNextLayoutRunnable(mRunnable);
        verify(mRunnable, never()).run();

        mTabImageView.layout(0, 0, 100, 100);
        assertFalse(mTabImageView.isLayoutRequested());
        verify(mRunnable, times(1)).run();
    }

    @Test
    @SmallTest
    public void testEmulateForceAnimationToFinish() {
        mRootView.addView(mTabImageView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        ShadowLooper.runUiThreadTasks();
        assertTrue(mTabImageView.isAttachedToWindow());

        mTabImageView.requestLayout();
        assertTrue(mTabImageView.isLayoutRequested());
        mTabImageView.setOnNextLayoutRunnable(mRunnable);
        verify(mRunnable, never()).run();

        mTabImageView.runOnNextLayoutRunnable();
        verify(mRunnable, times(1)).run();
    }

    @Test
    @SmallTest
    public void testAvoidsReentrantCalls() {
        mRootView.addView(mTabImageView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        ShadowLooper.runUiThreadTasks();
        assertTrue(mTabImageView.isAttachedToWindow());

        mTabImageView.requestLayout();
        assertTrue(mTabImageView.isLayoutRequested());
        // This validates that the runnable is cleared before invocation. If the runnable was not
        // cleared this implementation would recursively iterate until a timeout or the stack limit
        // was hit.
        mTabImageView.setOnNextLayoutRunnable(() -> {
            mRunnable.run();
            mTabImageView.runOnNextLayoutRunnable();
        });
        verify(mRunnable, never()).run();

        mTabImageView.runOnNextLayoutRunnable();
        verify(mRunnable, times(1)).run();
    }
}
