// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.pinned_tabs.PinnedTabStripAnimationManager.ItemState;
import org.chromium.ui.animation.AnimationHandler;

/** Unit tests for {@link PinnedTabStripAnimationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripAnimationManagerTest {
    private static final float DELTA = 1e-5f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AnimationHandler mAnimationHandler;

    private TabListRecyclerView mRecyclerView;
    private View mView;
    private PinnedTabStripAnimationManager mAnimationManager;
    private SettableNonNullObservableSupplier<Boolean> mAnimationRunningSupplier;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mRecyclerView = new TabListRecyclerView(activity, null);
        // Attach the view so that View#post() runs on the main looper.
        activity.setContentView(mRecyclerView);
        mAnimationManager = new PinnedTabStripAnimationManager(mRecyclerView, mAnimationHandler);
        mAnimationRunningSupplier = ObservableSuppliers.createNonNull(false);
        mView = new View(activity);
        mView.setLayoutParams(new ViewGroup.LayoutParams(100, 100));
    }

    @Test
    public void testAnimateShow_AlreadyVisible() {
        mRecyclerView.setVisibility(View.VISIBLE);

        mAnimationManager.animatePinnedTabBarVisibility(true, mAnimationRunningSupplier);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAnimationHandler, never()).startAnimation(any());
        assertFalse(mAnimationRunningSupplier.get());
    }

    @Test
    public void testAnimateShow_NotVisible() {
        mRecyclerView.setVisibility(View.GONE);

        mAnimationManager.animatePinnedTabBarVisibility(true, mAnimationRunningSupplier);
        // The view is made invisible so that it has a valid size before animating.
        assertEquals(View.INVISIBLE, mRecyclerView.getVisibility());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAnimationHandler).startAnimation(any());
        assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
    }

    @Test
    public void testAnimateHide_AlreadyHidden() {
        mRecyclerView.setVisibility(View.GONE);

        mAnimationManager.animatePinnedTabBarVisibility(false, mAnimationRunningSupplier);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAnimationHandler, never()).startAnimation(any());
        assertFalse(mAnimationRunningSupplier.get());
    }

    @Test
    public void testAnimateHide_Visible() {
        mRecyclerView.setVisibility(View.VISIBLE);

        mAnimationManager.animatePinnedTabBarVisibility(false, mAnimationRunningSupplier);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mAnimationHandler).startAnimation(any());
    }

    @Test
    public void testCancelAnimations() {
        mRecyclerView.setVisibility(View.GONE);
        mRecyclerView.setAlpha(0.5f);
        mRecyclerView.setClipBounds(new Rect(0, 0, 10, 10));
        mAnimationRunningSupplier.set(true);

        mAnimationManager.cancelPinnedTabBarAnimations(mAnimationRunningSupplier);
        verify(mAnimationHandler).forceFinishAnimation();
        assertEquals(View.VISIBLE, mRecyclerView.getVisibility());
        assertEquals(1.0f, mRecyclerView.getAlpha(), DELTA);
        assertNull(mRecyclerView.getClipBounds());
        assertFalse(mAnimationRunningSupplier.get());
    }

    @Test
    public void testAnimateItemWidth_NoChange() {
        mView.layout(0, 0, 100, 100);
        PinnedTabStripAnimationManager.animateItemWidth(mView, 100, mAnimationHandler);
        verify(mAnimationHandler, never()).startAnimation(any());
    }

    @Test
    public void testAnimateItemWidth_WithChange() {
        mView.layout(0, 0, 100, 100);
        PinnedTabStripAnimationManager.animateItemWidth(mView, 200, mAnimationHandler);
        verify(mAnimationHandler).startAnimation(any());
    }

    @Test
    public void testAnimateItemZoom_ToSelectedState() {
        mView.setScaleX(1.0f);
        mView.setScaleY(1.0f);
        mView.setAlpha(1.0f);

        ArgumentCaptor<ValueAnimator> animatorCaptor = ArgumentCaptor.forClass(ValueAnimator.class);
        PinnedTabStripAnimationManager.animateItemZoom(
                mView, ItemState.SELECTED, mAnimationHandler);
        verify(mAnimationHandler).startAnimation(animatorCaptor.capture());

        ValueAnimator animator = animatorCaptor.getValue();
        assertNotNull(animator);

        // Test at 50% animation progress
        animator.setCurrentFraction(0.5f);
        float expectedScale = 0.824442f;
        float expectedAlpha = 0.824442f;
        assertZoomState(expectedScale, expectedAlpha);

        // Test at 100% animation progress
        animator.setCurrentFraction(1.0f);
        assertZoomState(0.8f, 0.8f);
    }

    @Test
    public void testAnimateItemZoom_ToUnselectedState() {
        mView.setScaleX(0.8f);
        mView.setScaleY(0.8f);
        mView.setAlpha(0.8f);

        ArgumentCaptor<ValueAnimator> animatorCaptor = ArgumentCaptor.forClass(ValueAnimator.class);
        PinnedTabStripAnimationManager.animateItemZoom(
                mView, ItemState.UNSELECTED, mAnimationHandler);
        verify(mAnimationHandler).startAnimation(animatorCaptor.capture());

        ValueAnimator animator = animatorCaptor.getValue();
        assertNotNull(animator);

        // Test at 30% animation progress
        animator.setCurrentFraction(0.3f);
        float expectedScale = 0.93755436f;
        float expectedAlpha = 0.93755436f;
        assertZoomState(expectedScale, expectedAlpha);

        // Test at 100% animation progress
        animator.setCurrentFraction(1.0f);
        assertZoomState(1.0f, 1.0f);
    }

    private void assertZoomState(float expectedScale, float expectedAlpha) {
        assertEquals(expectedScale, mView.getScaleX(), DELTA);
        assertEquals(expectedScale, mView.getScaleY(), DELTA);
        assertEquals(expectedAlpha, mView.getAlpha(), DELTA);
    }
}
