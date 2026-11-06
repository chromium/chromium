// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.animation.ValueAnimator;
import android.content.Context;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.ui.GlicBottomSheetFreTransitionController.RevealCompleteCallback;
import org.chromium.chrome.browser.glic.ui.GlicBottomSheetFreTransitionController.RevealResult;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link GlicBottomSheetFreTransitionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.GLIC_NATIVE_FRE_ANDROID)
public class GlicBottomSheetFreTransitionControllerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private RevealCompleteCallback mRevealCallback;

    private static final long CROSSFADE_ADVANCE_MS =
            GlicBottomSheetFreTransitionController.CROSSFADE_DURATION_MS + 50;

    private View mFreRootView;
    private View mContentContainer;
    private View mSkeletonContainer;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mFreRootView = new View(context);
        mContentContainer = new View(context);
        mSkeletonContainer = new View(context);
        mSkeletonContainer.setVisibility(View.GONE);
    }

    @After
    public void tearDown() {
        ValueAnimator.setDurationScale(1.0f);
    }

    private GlicBottomSheetFreTransitionController createController(
            boolean isWebUiReady, boolean isWebUiLoadError) {
        return new GlicBottomSheetFreTransitionController(
                mFreRootView,
                mContentContainer,
                mSkeletonContainer,
                isWebUiReady,
                isWebUiLoadError,
                mRevealCallback);
    }

    private void finishCrossfadeAndVerify(boolean wasSkeletonShown, @RevealResult int result) {
        ShadowLooper.idleMainLooper(CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        assertEquals(View.GONE, mFreRootView.getVisibility());
        verify(mRevealCallback).onRevealComplete(wasSkeletonShown, result);
    }

    @Test
    public void testWarmPath_CrossfadesImmediatelyWithoutSkeleton() {
        mFreRootView.setClickable(true);
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ true, /* isWebUiLoadError= */ false);

        controller.onAccepted();

        assertFalse(mFreRootView.isClickable());
        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                mFreRootView.getImportantForAccessibility());
        assertEquals(View.GONE, mSkeletonContainer.getVisibility());
        verifyNoInteractions(mRevealCallback);

        finishCrossfadeAndVerify(/* wasSkeletonShown= */ false, RevealResult.READY);
    }

    @Test
    public void testReducedMotion_WarmPathPostsCallbackAsynchronously() {
        ValueAnimator.setDurationScale(0.0f);
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ true, /* isWebUiLoadError= */ false);

        controller.onAccepted();

        assertEquals(View.GONE, mFreRootView.getVisibility());
        assertEquals(0.0f, mFreRootView.getAlpha(), 0.01f);
        verifyNoInteractions(mRevealCallback);

        ShadowLooper.idleMainLooper();
        verify(mRevealCallback).onRevealComplete(/* wasSkeletonShown= */ false, RevealResult.READY);
    }

    @Test
    public void testWebUiStateChanged_WhileIdle() {
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ true, /* isWebUiLoadError= */ false);

        controller.onWebUiStateChanged(/* isReady= */ false, /* isError= */ false);
        controller.onAccepted();

        assertEquals(View.VISIBLE, mSkeletonContainer.getVisibility());
        verifyNoInteractions(mRevealCallback);
    }

    @Test
    public void testColdPath_ShowsSkeletonThenCrossfadesOnReady() {
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ false, /* isWebUiLoadError= */ false);

        controller.onAccepted();

        assertEquals(View.GONE, mContentContainer.getVisibility());
        assertEquals(View.VISIBLE, mSkeletonContainer.getVisibility());
        verifyNoInteractions(mRevealCallback);

        controller.onWebUiStateChanged(/* isReady= */ true, /* isError= */ false);
        verifyNoInteractions(mRevealCallback);

        finishCrossfadeAndVerify(/* wasSkeletonShown= */ true, RevealResult.READY);

        controller.onWebUiStateChanged(/* isReady= */ false, /* isError= */ true);
        ShadowLooper.idleMainLooper(CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        verifyNoMoreInteractions(mRevealCallback);
    }

    @Test
    public void testColdPath_WebUiLoadErrorRevealsErrorState() {
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ false, /* isWebUiLoadError= */ false);

        controller.onAccepted();
        verifyNoInteractions(mRevealCallback);

        controller.onWebUiStateChanged(/* isReady= */ false, /* isError= */ true);
        verifyNoInteractions(mRevealCallback);

        finishCrossfadeAndVerify(/* wasSkeletonShown= */ true, RevealResult.LOAD_ERROR);
    }

    @Test
    public void testColdPath_SkeletonTimeoutRevealsErrorState() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.GLIC_NATIVE_FRE_ANDROID,
                ChromeFeatureList.GLIC_NATIVE_FRE_ANDROID_SKELETON_TIMEOUT_MS,
                2500);
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ false, /* isWebUiLoadError= */ false);

        controller.onAccepted();

        assertEquals(View.VISIBLE, mSkeletonContainer.getVisibility());
        verifyNoInteractions(mRevealCallback);

        ShadowLooper.idleMainLooper(2400, TimeUnit.MILLISECONDS);
        verifyNoInteractions(mRevealCallback);

        ShadowLooper.idleMainLooper(100 + CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        assertEquals(View.GONE, mFreRootView.getVisibility());
        verify(mRevealCallback)
                .onRevealComplete(/* wasSkeletonShown= */ true, RevealResult.TIMED_OUT);
    }

    @Test
    public void testConstructedWithLoadError_TakesWarmErrorPath() {
        GlicBottomSheetFreTransitionController controller =
                createController(/* isWebUiReady= */ false, /* isWebUiLoadError= */ true);

        controller.onAccepted();

        assertEquals(View.GONE, mSkeletonContainer.getVisibility());
        verifyNoInteractions(mRevealCallback);

        finishCrossfadeAndVerify(/* wasSkeletonShown= */ false, RevealResult.LOAD_ERROR);
    }

    @Test
    public void testDestroy_CancelsCallbacksDuringSkeletonAndCrossfade() {
        GlicBottomSheetFreTransitionController coldController =
                createController(/* isWebUiReady= */ false, /* isWebUiLoadError= */ false);
        coldController.onAccepted();
        coldController.destroy();
        ShadowLooper.idleMainLooper(20000, TimeUnit.MILLISECONDS);
        verifyNoInteractions(mRevealCallback);

        GlicBottomSheetFreTransitionController warmController =
                createController(/* isWebUiReady= */ true, /* isWebUiLoadError= */ false);
        warmController.onAccepted();
        warmController.destroy();
        ShadowLooper.idleMainLooper(1000, TimeUnit.MILLISECONDS);
        assertEquals(View.VISIBLE, mFreRootView.getVisibility());
        verifyNoInteractions(mRevealCallback);

        ValueAnimator.setDurationScale(0.0f);
        GlicBottomSheetFreTransitionController reducedMotionController =
                createController(/* isWebUiReady= */ true, /* isWebUiLoadError= */ false);
        reducedMotionController.onAccepted();
        reducedMotionController.destroy();
        ShadowLooper.idleMainLooper();
        verifyNoInteractions(mRevealCallback);
    }
}
