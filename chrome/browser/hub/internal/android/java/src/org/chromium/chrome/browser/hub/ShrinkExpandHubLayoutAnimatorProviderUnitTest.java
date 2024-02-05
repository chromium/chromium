// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;
import static org.chromium.base.MathUtils.EPSILON;
import static org.chromium.chrome.browser.hub.HubLayoutConstants.EXPAND_NEW_TAB_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubLayoutConstants.SHRINK_EXPAND_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubLayoutConstants.TIMEOUT_MS;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.NonNull;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.hub.ShrinkExpandHubLayoutAnimatorProvider.ImageViewWeakRefBitmapCallback;
import org.chromium.ui.base.TestActivity;

import java.lang.ref.WeakReference;
import java.util.function.DoubleConsumer;

/** Unit tests for {@link ShrinkExpandHubLayoutAnimatorProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ShrinkExpandHubLayoutAnimatorProviderUnitTest {
    private static final int WIDTH = 100;
    private static final int HEIGHT = 1000;

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Spy private HubLayoutAnimationListener mListener;
    @Mock private Runnable mRunnableMock;
    @Mock private ImageView mImageViewMock;
    @Mock private Bitmap mBitmap;
    @Mock private DoubleConsumer mOnAlphaChange;

    private Activity mActivity;
    private FrameLayout mRootView;
    private HubContainerView mHubContainerView;
    private SyncOneshotSupplierImpl<ShrinkExpandAnimationData> mAnimationDataSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
        ShadowLooper.runUiThreadTasks();
        mAnimationDataSupplier = new SyncOneshotSupplierImpl<>();
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(mRootView);

        mHubContainerView = new HubContainerView(mActivity);
        mHubContainerView.setVisibility(View.INVISIBLE);
        View hubLayout = LayoutInflater.from(activity).inflate(R.layout.hub_layout, null);
        mHubContainerView.addView(hubLayout);
        mRootView.addView(mHubContainerView);

        mHubContainerView.layout(0, 0, WIDTH, HEIGHT);
    }

    @Test
    @SmallTest
    public void testShrinkTab() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("GridTabSwitcher.FramePerSecond.Shrink")
                        .expectAnyRecord("GridTabSwitcher.MaxFrameInterval.Shrink")
                        .expectAnyRecord("Android.GridTabSwitcher.Animation.TotalDuration.Shrink")
                        .expectAnyRecord(
                                "Android.GridTabSwitcher.Animation.FirstFrameLatency.Shrink")
                        .build();
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createShrinkTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.BLUE,
                        SHRINK_EXPAND_DURATION_MS,
                        mOnAlphaChange);
        assertEquals(HubLayoutAnimationType.SHRINK_TAB, animatorProvider.getPlannedAnimationType());
        Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
        assertNotNull(thumbnailCallback);

        Size thumbnailSize = new Size(20, 85);
        Rect initialRect = new Rect(0, 0, WIDTH, HEIGHT);
        Rect finalRect = new Rect(50, 10, 70, 95);
        ShrinkExpandAnimationData data =
                new ShrinkExpandAnimationData(
                        initialRect, finalRect, thumbnailSize, /* useFallbackAnimation= */ false);

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        ShrinkExpandImageView imageView = getImageView(animatorProvider);
        setUpShrinkExpandListener(
                /* isShrink= */ true, imageView, initialRect, finalRect, /* hasBitmap= */ true);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        thumbnailCallback.onResult(mBitmap);
        mAnimationDataSupplier.set(data);

        ShadowLooper.runUiThreadTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testExpandTab() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("GridTabSwitcher.FramePerSecond.Expand")
                        .expectAnyRecord("GridTabSwitcher.MaxFrameInterval.Expand")
                        .expectAnyRecord("Android.GridTabSwitcher.Animation.TotalDuration.Expand")
                        .expectAnyRecord(
                                "Android.GridTabSwitcher.Animation.FirstFrameLatency.Expand")
                        .build();
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createExpandTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.RED,
                        SHRINK_EXPAND_DURATION_MS,
                        mOnAlphaChange);
        assertEquals(HubLayoutAnimationType.EXPAND_TAB, animatorProvider.getPlannedAnimationType());
        Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
        assertNotNull(thumbnailCallback);

        Size thumbnailSize = new Size(20, 85);
        Rect initialRect = new Rect(50, 10, 70, 95);
        Rect finalRect = new Rect(0, 0, WIDTH, HEIGHT);
        ShrinkExpandAnimationData data =
                new ShrinkExpandAnimationData(
                        initialRect, finalRect, thumbnailSize, /* useFallbackAnimation= */ false);

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        ShrinkExpandImageView imageView = getImageView(animatorProvider);
        setUpShrinkExpandListener(
                /* isShrink= */ false, imageView, initialRect, finalRect, /* hasBitmap= */ true);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        mAnimationDataSupplier.set(data);
        thumbnailCallback.onResult(mBitmap);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testNewTab() {
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createNewTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.RED,
                        EXPAND_NEW_TAB_DURATION_MS,
                        mOnAlphaChange);
        assertEquals(
                HubLayoutAnimationType.EXPAND_NEW_TAB, animatorProvider.getPlannedAnimationType());
        assertNull(animatorProvider.getThumbnailCallback());

        Rect initialRect = new Rect(100, 0, 101, 1);
        Rect finalRect = new Rect(10, 15, WIDTH - 10, HEIGHT - 15);
        ShrinkExpandAnimationData data =
                new ShrinkExpandAnimationData(
                        initialRect,
                        finalRect,
                        /* thumbnailSize= */ null,
                        /* useFallbackAnimation= */ false);
        mAnimationDataSupplier.set(data);

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        ShrinkExpandImageView imageView = getImageView(animatorProvider);
        setUpShrinkExpandListener(
                /* isShrink= */ false, imageView, initialRect, finalRect, /* hasBitmap= */ false);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        // No bitmap is required.

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
    }

    @Test
    @SmallTest
    public void testShrinkFallbackAnimationDueToTimeoutMissingData() {
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createShrinkTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.BLUE,
                        SHRINK_EXPAND_DURATION_MS,
                        mOnAlphaChange);

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        setUpFadeListener(/* initialAlpha= */ 0f, /* finalAlpha= */ 1f);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        animatorProvider.getThumbnailCallback().onResult(mBitmap);

        // Intentionally supply no data.

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
    }

    @Test
    @SmallTest
    public void testShrinkFallbackAnimationDueToTimeoutMissingBitmap() {
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createShrinkTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.BLUE,
                        SHRINK_EXPAND_DURATION_MS,
                        mOnAlphaChange);

        Size thumbnailSize = new Size(20, 85);
        Rect initialRect = new Rect(0, 0, WIDTH, HEIGHT);
        Rect finalRect = new Rect(50, 10, 70, 95);
        ShrinkExpandAnimationData data =
                new ShrinkExpandAnimationData(
                        initialRect, finalRect, thumbnailSize, /* useFallbackAnimation= */ false);
        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        setUpFadeListener(/* initialAlpha= */ 0f, /* finalAlpha= */ 1f);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        // Intentionally supply no bitmap.
        mAnimationDataSupplier.set(data);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
    }

    @Test
    @SmallTest
    public void testShrinkFallbackAnimationViaSupplierData() {
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createShrinkTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.BLUE,
                        SHRINK_EXPAND_DURATION_MS,
                        mOnAlphaChange);

        Size thumbnailSize = new Size(20, 85);
        Rect initialRect = new Rect(0, 0, WIDTH, HEIGHT);
        Rect finalRect = new Rect(50, 10, 70, 95);
        ShrinkExpandAnimationData data =
                new ShrinkExpandAnimationData(
                        initialRect, finalRect, thumbnailSize, /* useFallbackAnimation= */ true);
        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        setUpFadeListener(/* initialAlpha= */ 0f, /* finalAlpha= */ 1f);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        animatorProvider.getThumbnailCallback().onResult(mBitmap);
        mAnimationDataSupplier.set(data);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
    }

    @Test
    @SmallTest
    public void testExpandFallbackAnimationViaForcedToFinish() {
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createExpandTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.BLUE,
                        SHRINK_EXPAND_DURATION_MS,
                        mOnAlphaChange);

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        setUpFadeListener(/* initialAlpha= */ 1f, /* finalAlpha= */ 0f);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        // Intentionally supply no data or bitmap.

        runner.forceAnimationToFinish();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ true);
    }

    @Test
    @SmallTest
    public void testNewTabFallbackAnimation() {
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createNewTabAnimatorProvider(
                        mHubContainerView,
                        mAnimationDataSupplier,
                        Color.RED,
                        EXPAND_NEW_TAB_DURATION_MS,
                        mOnAlphaChange);

        HubLayoutAnimationRunner runner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);

        Rect initialRect = new Rect(50, 50, 51, 51);
        Rect finalRect = new Rect(0, 10, WIDTH, HEIGHT - 10);
        ShrinkExpandAnimationData data =
                new ShrinkExpandAnimationData(
                        initialRect,
                        finalRect,
                        /* thumbnailSize= */ null,
                        /* useFallbackAnimation= */ true);

        ShrinkExpandImageView imageView = getImageView(animatorProvider);
        setUpShrinkExpandListener(
                /* isShrink= */ false, imageView, initialRect, finalRect, /* hasBitmap= */ false);
        runner.addListener(mListener);
        runner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);

        mAnimationDataSupplier.set(data);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verifyFinalState(animatorProvider, /* wasForcedToFinish= */ false);
    }

    @Test
    @SmallTest
    public void testImageViewWeakRefBitmapCallback() {
        ImageViewWeakRefBitmapCallback weakRefCallback =
                new ImageViewWeakRefBitmapCallback(mImageViewMock, mRunnableMock);

        weakRefCallback.onResult(mBitmap);

        verify(mImageViewMock).setImageBitmap(eq(mBitmap));
        verify(mRunnableMock).run();
    }

    @Test
    @SmallTest
    public void testImageViewWeakRefBitmapCallbackGarbageCollection() {
        ImageView imageView = new ImageView(mActivity);
        WeakReference<ImageView> imageViewWeakRef = new WeakReference<>(imageView);
        Runnable runnable =
                new Runnable() {
                    @Override
                    public void run() {}
                };
        WeakReference<Runnable> runnableWeakRef = new WeakReference<>(runnable);

        ImageViewWeakRefBitmapCallback weakRefCallback =
                new ImageViewWeakRefBitmapCallback(imageView, runnable);
        assertFalse(canBeGarbageCollected(imageViewWeakRef));
        assertFalse(canBeGarbageCollected(runnableWeakRef));

        imageView = null;
        runnable = null;
        assertTrue(canBeGarbageCollected(imageViewWeakRef));
        assertTrue(canBeGarbageCollected(runnableWeakRef));

        System.gc();

        // Verify this doesn't crash.
        weakRefCallback.onResult(mBitmap);
    }

    @Test
    @SmallTest
    public void testImageViewWeakRefBitmapCallbackNoBitmapIfNoView() {
        ImageView imageView = new ImageView(mActivity);
        WeakReference<ImageView> imageViewWeakRef = new WeakReference<>(imageView);

        ImageViewWeakRefBitmapCallback weakRefCallback =
                new ImageViewWeakRefBitmapCallback(imageView, mRunnableMock);
        assertFalse(canBeGarbageCollected(imageViewWeakRef));

        imageView = null;
        assertTrue(canBeGarbageCollected(imageViewWeakRef));
        System.gc();

        // Verify this doesn't crash.
        weakRefCallback.onResult(mBitmap);
        verify(mRunnableMock, never()).run();
    }

    private void setUpShrinkExpandListener(
            boolean isShrink,
            @NonNull ShrinkExpandImageView imageView,
            @NonNull Rect initialRect,
            @NonNull Rect finalRect,
            boolean hasBitmap) {
        View toolbarView = mHubContainerView.findViewById(R.id.hub_toolbar);
        mListener =
                spy(
                        new HubLayoutAnimationListener() {
                            @Override
                            public void onStart() {
                                assertEquals(
                                        "HubContainerView should be visible",
                                        View.VISIBLE,
                                        mHubContainerView.getVisibility());
                                assertEquals(
                                        "HubContainerView should have two children the Hub layout"
                                                + " and the ShrinkExpandImageView",
                                        2,
                                        mHubContainerView.getChildCount());
                                assertEquals(
                                        "HubContainerView should not have custom alpha",
                                        1f,
                                        mHubContainerView.getAlpha(),
                                        EPSILON);
                                assertEquals(
                                        "ShrinkExpandImageView should be visible",
                                        View.VISIBLE,
                                        imageView.getVisibility());
                                if (hasBitmap) {
                                    assertEquals(
                                            "ShrinkExpandImageView has wrong bitmap",
                                            mBitmap,
                                            imageView.getBitmap());
                                } else {
                                    assertNull(
                                            "ShrinkExpandImageView should have no bitmap",
                                            imageView.getBitmap());
                                }
                                assertEquals(imageView, mHubContainerView.getChildAt(1));
                                assertImageViewRect(imageView, initialRect);
                                float expectedAlpha = isShrink ? 0.0f : 1.0f;
                                assertEquals(
                                        "Unexpected initial toolbar alpha",
                                        expectedAlpha,
                                        toolbarView.getAlpha(),
                                        EPSILON);
                            }

                            @Override
                            public void onEnd(boolean wasForcedToFinish) {
                                assertImageViewRect(imageView, finalRect);
                                float expectedAlpha = isShrink ? 1.0f : 0.0f;
                                assertEquals(
                                        "Unexpected final toolbar alpha",
                                        expectedAlpha,
                                        toolbarView.getAlpha(),
                                        EPSILON);
                            }

                            @Override
                            public void afterEnd() {
                                assertEquals(
                                        "HubContainerView's ShrinkExpandImageView should have been"
                                                + " removed",
                                        1,
                                        mHubContainerView.getChildCount());
                                assertEquals(
                                        "Toolbar alpha not reset",
                                        1.0f,
                                        mHubContainerView.findViewById(R.id.hub_toolbar).getAlpha(),
                                        EPSILON);
                            }
                        });
    }

    private void setUpFadeListener(float initialAlpha, float finalAlpha) {
        mListener =
                spy(
                        new HubLayoutAnimationListener() {
                            @Override
                            public void beforeStart() {
                                assertEquals(
                                        "HubContainerView should be visible",
                                        View.VISIBLE,
                                        mHubContainerView.getVisibility());
                                assertEquals(
                                        "HubContainerView initial alpha is wrong",
                                        initialAlpha,
                                        mHubContainerView.getAlpha(),
                                        EPSILON);
                                assertEquals(
                                        "HubContainerView has unexpected extra child",
                                        1,
                                        mHubContainerView.getChildCount());
                                verify(mOnAlphaChange, atLeast(1))
                                        .accept(AdditionalMatchers.eq(initialAlpha, EPSILON));
                            }

                            @Override
                            public void onEnd(boolean wasForcedToFinish) {
                                assertEquals(
                                        "HubContainerView should remain visible",
                                        View.VISIBLE,
                                        mHubContainerView.getVisibility());
                                assertEquals(
                                        "HubContainerView final alpha is wrong",
                                        finalAlpha,
                                        mHubContainerView.getAlpha(),
                                        EPSILON);
                                verify(mOnAlphaChange, atLeast(1))
                                        .accept(AdditionalMatchers.eq(finalAlpha, EPSILON));

                                // At this point, there should have been a bunch of alpha change
                                // values somewhere between initial and final alpha. Verify we saw
                                // something in the middle half.
                                float middleAlpha = (initialAlpha + finalAlpha) / 2;
                                float halfRange = Math.abs(initialAlpha - finalAlpha) / 2;
                                verify(mOnAlphaChange, atLeast(1))
                                        .accept(AdditionalMatchers.eq(middleAlpha, halfRange));
                            }

                            @Override
                            public void afterEnd() {
                                assertEquals(
                                        "HubContainerView alpha was not reset",
                                        1.0f,
                                        mHubContainerView.getAlpha(),
                                        EPSILON);
                            }
                        });
    }

    private ShrinkExpandImageView getImageView(
            @NonNull HubLayoutAnimatorProvider animatorProvider) {
        if (animatorProvider
                instanceof ShrinkExpandHubLayoutAnimatorProvider shrinkExpandAnimatorProvider) {
            return shrinkExpandAnimatorProvider.getImageViewForTesting();
        } else {
            fail("Unexpected animatorProvider type.");
            return null;
        }
    }

    private void verifyFinalState(
            @NonNull HubLayoutAnimatorProvider animatorProvider, boolean wasForcedToFinish) {
        verify(mListener).beforeStart();
        verify(mListener).onEnd(eq(wasForcedToFinish));
        verify(mListener).afterEnd();

        assertNull("ShrinkExpandImageView should now be null", getImageView(animatorProvider));
        assertEquals(
                "HubContainerView's ShrinkExpandImageView child should have been removed",
                1,
                mHubContainerView.getChildCount());
    }

    private void assertImageViewRect(@NonNull ShrinkExpandImageView imageView, @NonNull Rect rect) {
        FrameLayout.LayoutParams params = (FrameLayout.LayoutParams) imageView.getLayoutParams();
        assertEquals("Width mismatch", rect.width(), params.width);
        assertEquals("Height mismatch", rect.height(), params.height);
        assertEquals("Left margin mismatch", rect.left, params.leftMargin);
        assertEquals("Top margin mismatch", rect.top, params.topMargin);
    }
}
