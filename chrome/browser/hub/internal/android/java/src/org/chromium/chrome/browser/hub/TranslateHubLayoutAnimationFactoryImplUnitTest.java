// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
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
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TranslateHubLayoutAnimationFactoryImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TranslateHubLayoutAnimationFactoryImplUnitTest {
    private static final long DURATION_MS = 500L;
    private static final long TIMEOUT_MS = 100L;
    private static final float FLOAT_TOLERANCE = 0.001f;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Spy private HubLayoutAnimationListener mListener;

    @Mock private ScrimController mScrimController;

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

                            // Force a layout to ensure width and height are defined.
                            mHubContainerView.layout(0, 0, 100, 100);
                        });
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    @SmallTest
    public void testTranslateUp() {
        HubLayoutAnimatorProvider animatorProvider =
                TranslateHubLayoutAnimationFactory.createTranslateUpAnimatorProvider(
                        mHubContainerView, mScrimController, DURATION_MS, 50);
        assertEquals(
                HubLayoutAnimationType.TRANSLATE_UP, animatorProvider.getPlannedAnimationType());

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        mListener =
                spy(
                        new HubLayoutAnimationListener() {
                            @Override
                            public void beforeStart() {
                                verify(mScrimController).startShowingScrim();
                                assertEquals(View.VISIBLE, mHubContainerView.getVisibility());
                                assertEquals(
                                        mHubContainerView.getHeight(),
                                        mHubContainerView.getY(),
                                        FLOAT_TOLERANCE);
                            }

                            @Override
                            public void onEnd(boolean wasForcedToFinish) {
                                assertEquals(View.VISIBLE, mHubContainerView.getVisibility());
                                assertEquals(50f, mHubContainerView.getY(), FLOAT_TOLERANCE);
                            }
                        });
        runner.addListener(mListener);

        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mListener).beforeStart();
        verify(mListener).onEnd(eq(false));
        verify(mScrimController, never()).startHidingScrim();
    }

    @Test
    @SmallTest
    public void testTranslateDown() {
        // Ensure the view is visible for hide.
        mHubContainerView.setVisibility(View.VISIBLE);
        ShadowLooper.runUiThreadTasks();

        HubLayoutAnimatorProvider animatorProvider =
                TranslateHubLayoutAnimationFactory.createTranslateDownAnimatorProvider(
                        mHubContainerView, mScrimController, DURATION_MS, 50);
        assertEquals(
                HubLayoutAnimationType.TRANSLATE_DOWN, animatorProvider.getPlannedAnimationType());

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        mListener =
                spy(
                        new HubLayoutAnimationListener() {
                            @Override
                            public void beforeStart() {
                                verify(mScrimController).startHidingScrim();
                                assertEquals(0.0f, mHubContainerView.getY(), FLOAT_TOLERANCE);
                            }

                            @Override
                            public void onEnd(boolean wasForcedToFinish) {
                                assertEquals(
                                        mHubContainerView.getHeight(),
                                        mHubContainerView.getY(),
                                        FLOAT_TOLERANCE);
                            }

                            @Override
                            public void afterEnd() {
                                assertEquals(50f, mHubContainerView.getY(), FLOAT_TOLERANCE);
                            }
                        });
        runner.addListener(mListener);

        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mListener).beforeStart();
        verify(mListener).onEnd(eq(false));
        verify(mListener).afterEnd();
        verify(mScrimController, never()).startShowingScrim();
    }
}
