// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.AdditionalMatchers;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link FadeHubLayoutAnimationFactoryImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public class FadeHubLayoutAnimationFactoryImplUnitTest {
    private static final long DURATION_MS = 500L;
    private static final long TIMEOUT_MS = 100L;
    private static final float FLOAT_TOLERANCE = 0.001f;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Spy private HubLayoutAnimationListener mListener;
    @Mock private DoubleConsumer mOnAlphaChange;

    private Activity mActivity;
    private FrameLayout mRootView;
    private HubContainerView mHubContainerView;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mRootView = new FrameLayout(mActivity);
                            mActivity.setContentView(mRootView);

                            mHubContainerView = new HubContainerView(mActivity);
                            mHubContainerView.setVisibility(View.INVISIBLE);
                            mRootView.addView(mHubContainerView);
                        });
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    @SmallTest
    public void testFadeIn() {
        HubLayoutAnimatorProvider animatorProvider =
                FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                        mHubContainerView, DURATION_MS, mOnAlphaChange);
        assertEquals(HubLayoutAnimationType.FADE_IN, animatorProvider.getPlannedAnimationType());

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        mListener =
                spy(
                        new HubLayoutAnimationListener() {
                            @Override
                            public void beforeStart() {
                                assertEquals(View.VISIBLE, mHubContainerView.getVisibility());
                                assertEquals(0.0f, mHubContainerView.getAlpha(), FLOAT_TOLERANCE);
                                verify(mOnAlphaChange, atLeast(1)).accept(0);
                            }

                            @Override
                            public void onEnd(boolean wasForcedToFinish) {
                                assertEquals(View.VISIBLE, mHubContainerView.getVisibility());
                                assertEquals(1.0f, mHubContainerView.getAlpha(), FLOAT_TOLERANCE);
                                verify(mOnAlphaChange, atLeast(1)).accept(1);
                                verifyMiddleAlpha(0, 1);
                            }
                        });
        runner.addListener(mListener);

        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mListener).beforeStart();
        verify(mListener).onEnd(eq(false));
        verify(mOnAlphaChange, atLeast(3)).accept(anyDouble());
    }

    @Test
    @SmallTest
    public void testFadeOut() {
        HubLayoutAnimatorProvider animatorProvider =
                FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                        mHubContainerView, DURATION_MS, mOnAlphaChange);
        assertEquals(HubLayoutAnimationType.FADE_OUT, animatorProvider.getPlannedAnimationType());

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        HubLayoutAnimationListener mListener =
                spy(
                        new HubLayoutAnimationListener() {
                            @Override
                            public void beforeStart() {
                                assertEquals(View.VISIBLE, mHubContainerView.getVisibility());
                                assertEquals(1.0f, mHubContainerView.getAlpha(), FLOAT_TOLERANCE);
                                verify(mOnAlphaChange, atLeast(1)).accept(1);
                            }

                            @Override
                            public void onEnd(boolean wasForcedToFinish) {
                                assertEquals(View.VISIBLE, mHubContainerView.getVisibility());
                                assertEquals(0.0f, mHubContainerView.getAlpha(), FLOAT_TOLERANCE);
                                verify(mOnAlphaChange, atLeast(1)).accept(0);
                                verifyMiddleAlpha(1, 0);
                            }

                            @Override
                            public void afterEnd() {
                                assertEquals(1.0f, mHubContainerView.getAlpha(), FLOAT_TOLERANCE);
                            }
                        });
        runner.addListener(mListener);

        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mListener).beforeStart();
        verify(mListener).onEnd(eq(false));
        verify(mListener).afterEnd();
        verify(mOnAlphaChange, atLeast(3)).accept(anyDouble());
    }

    private void verifyMiddleAlpha(float alpha1, float alpha2) {
        float middleAlpha = (alpha1 + alpha2) / 2;
        float halfRange = Math.abs(alpha1 - alpha2) / 2;
        verify(mOnAlphaChange, atLeast(1)).accept(AdditionalMatchers.eq(middleAlpha, halfRange));
    }
}
