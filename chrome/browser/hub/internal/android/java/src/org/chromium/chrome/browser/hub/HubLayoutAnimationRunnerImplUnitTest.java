// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.animation.AnimatorSet;
import android.animation.ValueAnimator;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.HubLayoutAnimationRunner.AnimationState;

/** Unit tests for {@link HubLayoutAnimationRunnerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubLayoutAnimationRunnerImplUnitTest {
    private static final long TIMEOUT_MS = 3000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HubLayoutAnimatorProvider mAnimatorProvider;
    @Mock private HubLayoutAnimationListener mAnimatorListener;
    @Mock private HubLayoutAnimationListener mListener;

    private final SyncOneshotSupplierImpl<HubLayoutAnimator> mAnimatorSupplier =
            new SyncOneshotSupplierImpl<>();
    private HubLayoutAnimationRunner mRunner;
    private HubLayoutAnimator mAnimator;

    @Before
    public void setUp() {
        when(mAnimatorProvider.getPlannedAnimationType())
                .thenReturn(HubLayoutAnimationType.TRANSLATE_UP);
        when(mAnimatorProvider.getAnimatorSupplier()).thenReturn(mAnimatorSupplier);

        mRunner = HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(mAnimatorProvider);

        assertEquals(AnimationState.INITIALIZING, mRunner.getAnimationState());
        assertEquals(HubLayoutAnimationType.TRANSLATE_UP, mRunner.getAnimationType());

        ValueAnimator animator = ValueAnimator.ofFloat(0.0f, 1.0f);
        animator.setDuration(1000L);
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.play(animator);

        mAnimator =
                new HubLayoutAnimator(
                        HubLayoutAnimationType.TRANSLATE_UP, animatorSet, mAnimatorListener);
    }

    @Test
    @SmallTest
    public void testRunWithImmediateAnimator_NoListeners() {
        HubLayoutAnimator animator =
                new HubLayoutAnimator(
                        HubLayoutAnimationType.FADE_IN, mAnimator.getAnimatorSet(), null);

        mAnimatorSupplier.set(animator);
        mRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        assertEquals(AnimationState.WAITING_FOR_ANIMATOR, mRunner.getAnimationState());
        assertEquals(HubLayoutAnimationType.TRANSLATE_UP, mRunner.getAnimationType());

        ShadowLooper.runMainLooperOneTask();

        verify(mAnimatorListener, never()).beforeStart();
        verify(mListener, never()).beforeStart();

        assertEquals(AnimationState.STARTED, mRunner.getAnimationState());
        assertEquals(HubLayoutAnimationType.FADE_IN, mRunner.getAnimationType());
        verify(mAnimatorListener, never()).onStart();
        verify(mListener, never()).onStart();

        mAnimator.getAnimatorSet().end();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());
        assertEquals(HubLayoutAnimationType.FADE_IN, mRunner.getAnimationType());
        verify(mAnimatorListener, never()).onEnd(eq(false));
        verify(mListener, never()).onEnd(eq(false));
        verify(mAnimatorListener, never()).afterEnd();
        verify(mListener, never()).afterEnd();
    }

    @Test
    @SmallTest
    public void testRunWithAsyncAnimator_AnimatorListenerOnly() {
        mRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);
        assertEquals(AnimationState.WAITING_FOR_ANIMATOR, mRunner.getAnimationState());

        mAnimatorSupplier.set(mAnimator);

        verify(mAnimatorListener).beforeStart();
        verify(mListener, never()).beforeStart();

        assertEquals(AnimationState.STARTED, mRunner.getAnimationState());

        verify(mAnimatorListener).onStart();
        verify(mListener, never()).onStart();

        mAnimator.getAnimatorSet().end();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());
        verify(mAnimatorListener).onEnd(eq(false));
        verify(mListener, never()).onEnd(eq(false));
        verify(mAnimatorListener).afterEnd();
        verify(mListener, never()).afterEnd();
    }

    @Test
    @SmallTest
    public void testRunOnTimeout_TwoListeners() {
        doRunnable(() -> mAnimatorSupplier.set(mAnimator))
                .when(mAnimatorProvider)
                .supplyAnimatorNow();

        mRunner.addListener(mListener);
        mRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);
        assertEquals(AnimationState.WAITING_FOR_ANIMATOR, mRunner.getAnimationState());

        // Simulate a timeout.
        ShadowLooper.runMainLooperOneTask();

        verify(mAnimatorProvider).supplyAnimatorNow();

        verify(mAnimatorListener).beforeStart();
        verify(mListener).beforeStart();

        assertEquals(AnimationState.STARTED, mRunner.getAnimationState());

        verify(mAnimatorListener).onStart();
        verify(mListener).onStart();

        mAnimator.getAnimatorSet().end();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());
        verify(mAnimatorListener).onEnd(eq(false));
        verify(mListener).onEnd(eq(false));
        verify(mAnimatorListener).afterEnd();
        verify(mListener).afterEnd();
    }

    @Test
    @SmallTest
    public void testForceAnimationToFinish() {
        mRunner.addListener(mListener);

        mRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);
        assertEquals(AnimationState.WAITING_FOR_ANIMATOR, mRunner.getAnimationState());

        mAnimatorSupplier.set(mAnimator);

        verify(mAnimatorListener).beforeStart();
        verify(mListener).beforeStart();

        assertEquals(AnimationState.STARTED, mRunner.getAnimationState());

        verify(mAnimatorListener).onStart();
        verify(mListener).onStart();

        mRunner.forceAnimationToFinish();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());
        verify(mAnimatorListener).onEnd(eq(true));
        verify(mListener).onEnd(eq(true));
        verify(mAnimatorListener).afterEnd();
        verify(mListener).afterEnd();
    }

    @Test
    @SmallTest
    public void testForceAnimationToFinishWithPostedOnAnimationReadyNoWaitForTask() {
        mRunner.addListener(mListener);

        mAnimatorSupplier.set(mAnimator);
        mRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);
        assertEquals(AnimationState.WAITING_FOR_ANIMATOR, mRunner.getAnimationState());

        mRunner.forceAnimationToFinish();

        verify(mAnimatorListener).beforeStart();
        verify(mListener).beforeStart();

        verify(mAnimatorListener).onStart();
        verify(mListener).onStart();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());
        verify(mAnimatorListener).onEnd(eq(true));
        verify(mListener).onEnd(eq(true));
        verify(mAnimatorListener).afterEnd();
        verify(mListener).afterEnd();
    }

    @Test
    @SmallTest
    public void testForceAnimationToFinishWithNoAnimationSupplied() {
        doRunnable(() -> mAnimatorSupplier.set(mAnimator))
                .when(mAnimatorProvider)
                .supplyAnimatorNow();

        mRunner.addListener(mListener);
        mRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);
        assertEquals(AnimationState.WAITING_FOR_ANIMATOR, mRunner.getAnimationState());

        mRunner.forceAnimationToFinish();

        verify(mAnimatorProvider).supplyAnimatorNow();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());

        verify(mAnimatorListener).beforeStart();
        verify(mListener).beforeStart();

        verify(mAnimatorListener).onStart();
        verify(mListener).onStart();

        verify(mAnimatorListener).onEnd(eq(true));
        verify(mListener).onEnd(eq(true));
        verify(mAnimatorListener).afterEnd();
        verify(mListener).afterEnd();
    }

    @Test
    @SmallTest
    public void testForceAnimationToFinishWithoutRunWithTimeout() {
        mRunner.addListener(mListener);

        assertEquals(AnimationState.INITIALIZING, mRunner.getAnimationState());

        mAnimatorSupplier.set(mAnimator);

        assertEquals(AnimationState.INITIALIZING, mRunner.getAnimationState());

        mRunner.forceAnimationToFinish();

        verify(mAnimatorListener).beforeStart();
        verify(mListener).beforeStart();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());

        verify(mAnimatorListener).onStart();
        verify(mListener).onStart();

        assertEquals(AnimationState.FINISHED, mRunner.getAnimationState());
        verify(mAnimatorListener).onEnd(eq(true));
        verify(mListener).onEnd(eq(true));
        verify(mAnimatorListener).afterEnd();
        verify(mListener).afterEnd();
    }
}
