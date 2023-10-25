// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link RunOnNextLayoutDelegate}. */
// TODO(crbug/1495731): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
public class ShrinkExpandImageViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private FrameLayout mRootView;
    private ShrinkExpandImageView mShrinkExpandImageView;

    @Mock private Runnable mRunnable1;
    @Mock private Runnable mRunnable2;

    @Before
    public void setUp() {
        // This setup is necessary to get isAttachedToWindow to work correctly.
        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivityController.setup();
        mActivity = mActivityController.get();

        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(mRootView);

        mShrinkExpandImageView = new ShrinkExpandImageView(mActivity);
    }

    @After
    public void tearDown() {
        mActivityController.destroy();
    }

    /** These tests are mirrored from {@link RunOnNextLayoutDelegateUnitTest}. */
    @Test
    @SmallTest
    public void testRunsImmediatelyWhenDetached() {
        assertFalse(mShrinkExpandImageView.isAttachedToWindow());

        mShrinkExpandImageView.runOnNextLayout(mRunnable1);
        verify(mRunnable1, times(1)).run();

        mShrinkExpandImageView.runOnNextLayout(mRunnable2);
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsImmediatelyIfNotWaitingForLayout() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mShrinkExpandImageView.isAttachedToWindow());
        assertFalse(mShrinkExpandImageView.isLayoutRequested());

        mShrinkExpandImageView.runOnNextLayout(mRunnable1);
        verify(mRunnable1, times(1)).run();

        mShrinkExpandImageView.runOnNextLayout(mRunnable2);
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsOnNextLayout() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mShrinkExpandImageView.isAttachedToWindow());

        mShrinkExpandImageView.requestLayout();
        assertTrue(mShrinkExpandImageView.isLayoutRequested());

        mShrinkExpandImageView.runOnNextLayout(mRunnable1);
        mShrinkExpandImageView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        mShrinkExpandImageView.layout(0, 0, 100, 100);
        assertFalse(mShrinkExpandImageView.isLayoutRequested());

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsWithoutALayout() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mShrinkExpandImageView.isAttachedToWindow());

        mShrinkExpandImageView.requestLayout();
        assertTrue(mShrinkExpandImageView.isLayoutRequested());

        mShrinkExpandImageView.runOnNextLayout(mRunnable1);
        mShrinkExpandImageView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        // Even if a layout never happens because the mShrinkExpandImageView hasn't changed, the
        // runnable should still run.
        ShadowLooper.runUiThreadTasks();

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testForceRunnablesToRun() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mShrinkExpandImageView.isAttachedToWindow());

        mShrinkExpandImageView.requestLayout();
        assertTrue(mShrinkExpandImageView.isLayoutRequested());

        mShrinkExpandImageView.runOnNextLayout(mRunnable1);
        mShrinkExpandImageView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        mShrinkExpandImageView.runOnNextLayoutRunnables();

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testAvoidsReentrantCalls() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mShrinkExpandImageView.isAttachedToWindow());

        mShrinkExpandImageView.requestLayout();
        assertTrue(mShrinkExpandImageView.isLayoutRequested());

        // This validates that the runnable is cleared before invocation. If the runnable was not
        // cleared this implementation would recursively iterate until a timeout or the stack limit
        // was hit.
        mShrinkExpandImageView.runOnNextLayout(
                () -> {
                    mRunnable1.run();
                    mShrinkExpandImageView.runOnNextLayoutRunnables();
                });
        verify(mRunnable1, never()).run();

        mShrinkExpandImageView.runOnNextLayoutRunnables();
        verify(mRunnable1, times(1)).run();

        mShrinkExpandImageView.runOnNextLayoutRunnables();
        verify(mRunnable1, times(1)).run();
    }
}
