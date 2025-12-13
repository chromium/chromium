// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.animation.ValueAnimator;
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
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip.PinnedTabStripAnimationManager.ItemState;
import org.chromium.ui.animation.AnimationHandler;

/** Unit tests for {@link PinnedTabStripAnimationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PinnedTabStripAnimationManagerTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListRecyclerView mRecyclerView;
    @Mock private AnimationHandler mAnimationHandler;
    @Mock private View mView;
    @Mock private ViewGroup.LayoutParams mLayoutParams;

    private PinnedTabStripAnimationManager mAnimationManager;
    private ObservableSupplierImpl<Boolean> mAnimationRunningSupplier;

    @Before
    public void setUp() {
        mAnimationManager = new PinnedTabStripAnimationManager(mRecyclerView, mAnimationHandler);
        mAnimationRunningSupplier = new ObservableSupplierImpl<>(false);
        when(mView.getLayoutParams()).thenReturn(mLayoutParams);
    }

    @Test
    public void testAnimateShow_AlreadyVisible() {
        when(mRecyclerView.getVisibility()).thenReturn(View.VISIBLE);
        doAnswer(
                        invocation -> {
                            invocation.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mRecyclerView)
                .post(any(Runnable.class));

        mAnimationManager.animatePinnedTabBarVisibility(true, mAnimationRunningSupplier);

        verify(mAnimationHandler, never()).startAnimation(any());
        assertFalse(mAnimationRunningSupplier.get());
    }

    @Test
    public void testAnimateShow_NotVisible() {
        when(mRecyclerView.getVisibility()).thenReturn(View.GONE);
        doAnswer(
                        invocation -> {
                            invocation.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mRecyclerView)
                .post(any(Runnable.class));

        mAnimationManager.animatePinnedTabBarVisibility(true, mAnimationRunningSupplier);

        verify(mRecyclerView).setVisibility(View.INVISIBLE);
        verify(mAnimationHandler).startAnimation(any());
    }

    @Test
    public void testAnimateHide_AlreadyHidden() {
        when(mRecyclerView.getVisibility()).thenReturn(View.GONE);
        doAnswer(
                        invocation -> {
                            invocation.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mRecyclerView)
                .post(any(Runnable.class));

        mAnimationManager.animatePinnedTabBarVisibility(false, mAnimationRunningSupplier);

        verify(mAnimationHandler, never()).startAnimation(any());
        assertFalse(mAnimationRunningSupplier.get());
    }

    @Test
    public void testAnimateHide_Visible() {
        when(mRecyclerView.getVisibility()).thenReturn(View.VISIBLE);
        doAnswer(
                        invocation -> {
                            invocation.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mRecyclerView)
                .post(any(Runnable.class));

        mAnimationManager.animatePinnedTabBarVisibility(false, mAnimationRunningSupplier);

        verify(mAnimationHandler).startAnimation(any());
    }

    @Test
    public void testCancelAnimations() {
        mAnimationManager.cancelPinnedTabBarAnimations(mAnimationRunningSupplier);
        verify(mAnimationHandler).forceFinishAnimation();
        verify(mRecyclerView).setVisibility(View.VISIBLE);
        verify(mRecyclerView).setAlpha(1.0f);
        verify(mRecyclerView).setClipBounds(null);
        assertFalse(mAnimationRunningSupplier.get());
    }

    @Test
    public void testAnimateItemWidth_NoChange() {
        when(mView.getWidth()).thenReturn(100);
        PinnedTabStripAnimationManager.animateItemWidth(mView, 100, mAnimationHandler);
        verify(mAnimationHandler, never()).startAnimation(any());
    }

    @Test
    public void testAnimateItemWidth_WithChange() {
        when(mView.getWidth()).thenReturn(100);
        PinnedTabStripAnimationManager.animateItemWidth(mView, 200, mAnimationHandler);
        verify(mAnimationHandler).startAnimation(any());
    }

    @Test
    public void testAnimateItemZoom_ToSelectedState() {
        when(mView.getScaleX()).thenReturn(1.0f);
        when(mView.getAlpha()).thenReturn(1.0f);

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
        verify(mView).setScaleX(expectedScale);
        verify(mView).setScaleY(expectedScale);
        verify(mView).setAlpha(expectedAlpha);

        // Test at 100% animation progress
        animator.setCurrentFraction(1.0f);
        verify(mView).setScaleX(0.8f);
        verify(mView).setScaleY(0.8f);
        verify(mView).setAlpha(0.8f);
    }

    @Test
    public void testAnimateItemZoom_ToUnselectedState() {
        when(mView.getScaleX()).thenReturn(0.8f);
        when(mView.getAlpha()).thenReturn(0.8f);

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
        verify(mView).setScaleX(expectedScale);
        verify(mView).setScaleY(expectedScale);
        verify(mView).setAlpha(expectedAlpha);

        // Test at 100% animation progress
        animator.setCurrentFraction(1.0f);
        verify(mView).setScaleX(1.0f);
        verify(mView).setScaleY(1.0f);
        verify(mView).setAlpha(1.0f);
    }
}
