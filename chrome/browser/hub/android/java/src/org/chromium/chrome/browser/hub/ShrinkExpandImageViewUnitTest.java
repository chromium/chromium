// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.MathUtils.EPSILON;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
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
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;

/** Tests for {@link RunOnNextLayoutDelegate}. */
// TODO(crbug.com/40286625): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(Mode.PAUSED)
public class ShrinkExpandImageViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private FrameLayout mRootView;
    private ShrinkExpandImageView mShrinkExpandImageView;

    @Mock private Runnable mRunnable1;
    @Mock private Runnable mRunnable2;

    @Mock private Bitmap mBitmap;
    @Mock private Drawable mDrawable;

    @Before
    public void setUp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);

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

    @Test
    @SmallTest
    public void testReset() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);

        mShrinkExpandImageView.setScaleX(1.5f);
        mShrinkExpandImageView.setScaleY(2.0f);
        mShrinkExpandImageView.setTranslationX(3.0f);
        mShrinkExpandImageView.setTranslationY(3.0f);
        Matrix m = new Matrix();
        m.setTranslate(40, 60);
        mShrinkExpandImageView.setImageMatrix(m);
        mShrinkExpandImageView.setImageBitmap(mBitmap);
        assertEquals(mBitmap, mShrinkExpandImageView.getBitmap());

        int left = 100;
        int top = 50;
        int width = 70;
        int height = 1000;
        Rect rect = new Rect(left, top, left + width, top + height);
        mShrinkExpandImageView.reset(rect);

        assertReset(rect, /* keepingBitmap= */ false);
    }

    @Test
    @SmallTest
    public void testResetKeepingBitmap() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);

        mShrinkExpandImageView.setScaleX(1.5f);
        mShrinkExpandImageView.setScaleY(2.0f);
        mShrinkExpandImageView.setTranslationX(3.0f);
        mShrinkExpandImageView.setTranslationY(3.0f);
        Matrix m = new Matrix();
        m.setTranslate(40, 60);
        mShrinkExpandImageView.setImageMatrix(m);
        mShrinkExpandImageView.setImageBitmap(mBitmap);
        assertEquals(mBitmap, mShrinkExpandImageView.getBitmap());

        int left = 100;
        int top = 50;
        int width = 70;
        int height = 1000;
        Rect rect = new Rect(left, top, left + width, top + height);
        mShrinkExpandImageView.resetKeepingBitmap(rect);

        assertReset(rect, /* keepingBitmap= */ true);
    }

    @Test
    @SmallTest
    public void testGetBitmap() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 100, 100);

        mShrinkExpandImageView.setImageBitmap(mBitmap);
        assertEquals(mBitmap, mShrinkExpandImageView.getBitmap());

        mShrinkExpandImageView.setImageBitmap(null);
        assertNull(mShrinkExpandImageView.getBitmap());

        mShrinkExpandImageView.setImageBitmap(mBitmap);
        assertEquals(mBitmap, mShrinkExpandImageView.getBitmap());

        mShrinkExpandImageView.setImageDrawable(mDrawable);
        assertNull(mShrinkExpandImageView.getBitmap());

        mShrinkExpandImageView.setImageBitmap(mBitmap);
        assertEquals(mBitmap, mShrinkExpandImageView.getBitmap());

        mShrinkExpandImageView.setImageDrawable(null);
        assertNull(mShrinkExpandImageView.getBitmap());
    }

    /** These tests are mirrored from {@link RunOnNextLayoutDelegateUnitTest}. */
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
    public void testDelayedIfLayoutHasZeroDimension() {
        mRootView.addView(mShrinkExpandImageView);
        ShadowLooper.runUiThreadTasks();
        mRootView.layout(0, 0, 0, 100);
        assertTrue(mShrinkExpandImageView.isAttachedToWindow());

        mShrinkExpandImageView.requestLayout();
        assertTrue(mShrinkExpandImageView.isLayoutRequested());

        mShrinkExpandImageView.runOnNextLayout(mRunnable1);
        mShrinkExpandImageView.runOnNextLayout(mRunnable2);
        verify(mRunnable1, never()).run();
        verify(mRunnable2, never()).run();

        mShrinkExpandImageView.layout(0, 0, 100, 100);
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

    @Test
    @SmallTest
    public void testImageBitmapDensityOnAutomotive() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);

        Bitmap testBitmap = Bitmap.createBitmap(1000, 1000, Bitmap.Config.ARGB_8888);
        testBitmap.setDensity(DisplayMetrics.DENSITY_MEDIUM);
        mShrinkExpandImageView.setImageBitmap(testBitmap);
        assertEquals(
                "Bitmap density in automotive should be scaled up from 160 to 220.",
                DisplayMetrics.DENSITY_220,
                testBitmap.getDensity());
    }

    private void assertReset(@NonNull Rect rect, boolean keepingBitmap) {
        assertEquals(1.0f, mShrinkExpandImageView.getScaleX(), EPSILON);
        assertEquals(1.0f, mShrinkExpandImageView.getScaleY(), EPSILON);
        assertEquals(0.0f, mShrinkExpandImageView.getTranslationX(), EPSILON);
        assertEquals(0.0f, mShrinkExpandImageView.getTranslationY(), EPSILON);
        assertTrue(mShrinkExpandImageView.getImageMatrix().isIdentity());
        if (keepingBitmap) {
            assertEquals(mBitmap, mShrinkExpandImageView.getBitmap());
        } else {
            assertNull(mShrinkExpandImageView.getBitmap());
        }

        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mShrinkExpandImageView.getLayoutParams();
        assertEquals(rect.left, layoutParams.leftMargin);
        assertEquals(rect.top, layoutParams.topMargin);
        assertEquals(rect.width(), layoutParams.width);
        assertEquals(rect.height(), layoutParams.height);
    }
}
