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
import android.content.Context;
import android.view.View;
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
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
public class RunOnNextLayoutDelegateUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static class RunOnNextLayoutView extends View implements RunOnNextLayout {
        private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

        RunOnNextLayoutView(Context context) {
            super(context);
            mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
        }

        @Override
        public void layout(int l, int t, int r, int b) {
            super.layout(l, t, r, b);
            runOnNextLayoutRunnables();
        }

        @Override
        public void runOnNextLayout(Runnable r) {
            mRunOnNextLayoutDelegate.runOnNextLayout(r);
        }

        @Override
        public void runOnNextLayoutRunnables() {
            mRunOnNextLayoutDelegate.runOnNextLayoutRunnables();
        }
    }

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private FrameLayout mRootView;
    private RunOnNextLayoutView mRunOnNextLayoutView;

    @Mock private Runnable mRunnable1;
    @Mock private Runnable mRunnable2;

    /** Returns the activity to run the test on. */
    @Before
    public void setUp() {
        // This setup is necessary to get isAttachedToWindow to work correctly.
        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivityController.setup();
        mActivity = mActivityController.get();

        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(mRootView);

        mRunOnNextLayoutView = new RunOnNextLayoutView(mActivity);
    }

    @After
    public void tearDown() {
        mActivityController.destroy();
    }

    @Test
    @SmallTest
    public void testRunsImmediatelyIfNotWaitingForLayout() {
        mRootView.addView(mRunOnNextLayoutView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mRunOnNextLayoutView.isAttachedToWindow());
        assertFalse(mRunOnNextLayoutView.isLayoutRequested());

        mRunOnNextLayoutView.runOnNextLayout(mRunnable1);
        verify(mRunnable1, times(1)).run();

        mRunOnNextLayoutView.runOnNextLayout(mRunnable2);
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsOnNextLayout() {
        mRootView.addView(mRunOnNextLayoutView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mRunOnNextLayoutView.isAttachedToWindow());

        mRunOnNextLayoutView.requestLayout();
        assertTrue(mRunOnNextLayoutView.isLayoutRequested());

        mRunOnNextLayoutView.runOnNextLayout(mRunnable1);
        mRunOnNextLayoutView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        mRunOnNextLayoutView.layout(0, 0, 100, 100);
        assertFalse(mRunOnNextLayoutView.isLayoutRequested());

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testRunsWithoutALayout() {
        mRootView.addView(mRunOnNextLayoutView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mRunOnNextLayoutView.isAttachedToWindow());

        mRunOnNextLayoutView.requestLayout();
        assertTrue(mRunOnNextLayoutView.isLayoutRequested());

        mRunOnNextLayoutView.runOnNextLayout(mRunnable1);
        mRunOnNextLayoutView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        // Even if a layout never happens because the mRunOnNextLayoutView hasn't changed, the
        // runnable should still run.
        ShadowLooper.runUiThreadTasks();

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testDelayedIfLayoutHasZeroDimension() {
        mRootView.addView(mRunOnNextLayoutView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 0, 100);
        assertTrue(mRunOnNextLayoutView.isAttachedToWindow());

        mRunOnNextLayoutView.requestLayout();
        assertTrue(mRunOnNextLayoutView.isLayoutRequested());

        mRunOnNextLayoutView.runOnNextLayout(mRunnable1);
        mRunOnNextLayoutView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        mRunOnNextLayoutView.layout(0, 0, 100, 100);
        ShadowLooper.runUiThreadTasks();

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testForceRunnablesToRun() {
        mRootView.addView(mRunOnNextLayoutView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mRunOnNextLayoutView.isAttachedToWindow());

        mRunOnNextLayoutView.requestLayout();
        assertTrue(mRunOnNextLayoutView.isLayoutRequested());

        mRunOnNextLayoutView.runOnNextLayout(mRunnable1);
        mRunOnNextLayoutView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        mRunOnNextLayoutView.runOnNextLayoutRunnables();

        verify(mRunnable1, times(1)).run();
        verify(mRunnable2, times(1)).run();
    }

    @Test
    @SmallTest
    public void testAvoidsReentrantCalls() {
        mRootView.addView(mRunOnNextLayoutView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);
        assertTrue(mRunOnNextLayoutView.isAttachedToWindow());

        mRunOnNextLayoutView.requestLayout();
        assertTrue(mRunOnNextLayoutView.isLayoutRequested());

        // This validates that the runnable is cleared before invocation. If the runnable was not
        // cleared this implementation would recursively iterate until a timeout or the stack limit
        // was hit.
        mRunOnNextLayoutView.runOnNextLayout(
                () -> {
                    mRunnable1.run();
                    mRunOnNextLayoutView.runOnNextLayoutRunnables();
                });
        verify(mRunnable1, never()).run();

        mRunOnNextLayoutView.runOnNextLayoutRunnables();
        verify(mRunnable1, times(1)).run();

        mRunOnNextLayoutView.runOnNextLayoutRunnables();
        verify(mRunnable1, times(1)).run();
    }
}
