// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Looper;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for {@link HubPaneSwipeAnimator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneSwipeAnimatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HubPaneSwipeAnimator.SwipeAnimationProgressCallback mProgressCallback;

    private ActivityController<TestActivity> mActivityController;
    private HubPaneSwipeAnimator mAnimator;
    private View mCurrentView;
    private View mAdjacentView;
    private FrameLayout mContainer;

    private static final int CONTAINER_WIDTH = 1000;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();

        mContainer = new FrameLayout(activity);
        mCurrentView = new View(activity);
        mAdjacentView = new View(activity);

        mContainer.addView(mCurrentView);
        mContainer.addView(mAdjacentView);

        mContainer.setLayoutParams(new FrameLayout.LayoutParams(CONTAINER_WIDTH, 1000));
        mContainer.measure(
                View.MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mContainer.layout(0, 0, CONTAINER_WIDTH, 1000);

        mAnimator = new HubPaneSwipeAnimator();
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testAnimateSettle_switchLeft() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mCurrentView.setTranslationX(-300f);
        mAdjacentView.setTranslationX(700f);

        mAnimator.animateSettle(
                mCurrentView,
                mAdjacentView,
                /* isSwitch= */ true,
                /* isSwipeLeft= */ true,
                CONTAINER_WIDTH,
                mProgressCallback,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(-CONTAINER_WIDTH, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(0f, mAdjacentView.getTranslationX(), 0.01f);
        verify(mProgressCallback, atLeastOnce()).onProgress(anyFloat(), eq(true));
    }

    @Test
    public void testAnimateSettle_cancelLeft() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mCurrentView.setTranslationX(-100f);
        mAdjacentView.setTranslationX(900f);

        mAnimator.animateSettle(
                mCurrentView,
                mAdjacentView,
                /* isSwitch= */ false,
                /* isSwipeLeft= */ true,
                CONTAINER_WIDTH,
                mProgressCallback,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(0f, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(CONTAINER_WIDTH, mAdjacentView.getTranslationX(), 0.01f);
    }

    @Test
    public void testAnimateSettle_switchRight() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mCurrentView.setTranslationX(300f);
        mAdjacentView.setTranslationX(-700f);

        mAnimator.animateSettle(
                mCurrentView,
                mAdjacentView,
                /* isSwitch= */ true,
                /* isSwipeLeft= */ false,
                CONTAINER_WIDTH,
                mProgressCallback,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(CONTAINER_WIDTH, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(0f, mAdjacentView.getTranslationX(), 0.01f);
        verify(mProgressCallback, atLeastOnce()).onProgress(anyFloat(), eq(false));
    }

    @Test
    public void testAnimateSettle_cancelRight() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mCurrentView.setTranslationX(100f);
        mAdjacentView.setTranslationX(-900f);

        mAnimator.animateSettle(
                mCurrentView,
                mAdjacentView,
                /* isSwitch= */ false,
                /* isSwipeLeft= */ false,
                CONTAINER_WIDTH,
                mProgressCallback,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(0f, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(-CONTAINER_WIDTH, mAdjacentView.getTranslationX(), 0.01f);
    }

    @Test
    public void testAnimateSettle_zeroWidth() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mAnimator.animateSettle(
                mCurrentView,
                mAdjacentView,
                /* isSwitch= */ true,
                /* isSwipeLeft= */ true,
                /* containerWidth= */ 0,
                mProgressCallback,
                () -> ended.set(true));

        assertTrue(ended.get());
    }

    @Test
    public void testAnimateSlideTransition_rightToLeft() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mAnimator.animateSlideTransition(
                mContainer,
                mCurrentView,
                mAdjacentView,
                /* isLeftToRight= */ false,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(0f, mAdjacentView.getTranslationX(), 0.01f);
        assertEquals(0f, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(1, mContainer.getChildCount());
    }

    @Test
    public void testAnimateSlideTransition_leftToRight() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mAnimator.animateSlideTransition(
                mContainer,
                mCurrentView,
                mAdjacentView,
                /* isLeftToRight= */ true,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(0f, mAdjacentView.getTranslationX(), 0.01f);
        assertEquals(0f, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(1, mContainer.getChildCount());
    }

    @Test
    public void testAnimateSlideTransition_zeroWidth() {
        AtomicBoolean ended = new AtomicBoolean(false);
        FrameLayout emptyContainer = new FrameLayout(mActivityController.get());

        mAnimator.animateSlideTransition(
                emptyContainer,
                mCurrentView,
                mAdjacentView,
                /* isLeftToRight= */ true,
                () -> ended.set(true));

        assertTrue(ended.get());
    }

    @Test
    public void testAnimateSettle_nullProgressCallback() {
        AtomicBoolean ended = new AtomicBoolean(false);

        mCurrentView.setTranslationX(-300f);
        mAdjacentView.setTranslationX(700f);

        mAnimator.animateSettle(
                mCurrentView,
                mAdjacentView,
                /* isSwitch= */ true,
                /* isSwipeLeft= */ true,
                CONTAINER_WIDTH,
                /* progressCallback= */ null,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(-CONTAINER_WIDTH, mCurrentView.getTranslationX(), 0.01f);
        assertEquals(0f, mAdjacentView.getTranslationX(), 0.01f);
    }

    @Test
    public void testAnimateSlideTransition_newViewHasNoParent() {
        AtomicBoolean ended = new AtomicBoolean(false);
        mContainer.removeView(mAdjacentView);
        assertNull(mAdjacentView.getParent());

        mAnimator.animateSlideTransition(
                mContainer,
                mCurrentView,
                mAdjacentView,
                /* isLeftToRight= */ true,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(mContainer, mAdjacentView.getParent());
    }

    @Test
    public void testAnimateSlideTransition_newViewHasDifferentParent() {
        AtomicBoolean ended = new AtomicBoolean(false);
        mContainer.removeView(mAdjacentView);
        FrameLayout otherContainer = new FrameLayout(mActivityController.get());
        otherContainer.addView(mAdjacentView);
        assertEquals(otherContainer, mAdjacentView.getParent());

        mAnimator.animateSlideTransition(
                mContainer,
                mCurrentView,
                mAdjacentView,
                /* isLeftToRight= */ true,
                () -> ended.set(true));

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        assertTrue(ended.get());
        assertEquals(mContainer, mAdjacentView.getParent());
        assertEquals(0, otherContainer.getChildCount());
    }

    @Test
    public void testForceFinishAnimation() {
        // Calling forceFinishAnimation when no animation is running is a safe no-op.
        mAnimator.forceFinishAnimation();

        AtomicBoolean ended = new AtomicBoolean(false);
        mAnimator.animateSlideTransition(
                mContainer,
                mCurrentView,
                mAdjacentView,
                /* isLeftToRight= */ true,
                () -> ended.set(true));

        assertFalse(ended.get());
        mAnimator.forceFinishAnimation();
        assertTrue(ended.get());
    }
}
