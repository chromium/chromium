// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link ContinuousSearchContainerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchContainerMediatorTest {
    private ContinuousSearchContainerMediator mMediator;
    private PropertyModel mModel;
    private BrowserControlsStateProvider.Observer mCurrentBrowserControlsObserver;
    private int mCurrentTopControlsHeight;
    private int mCurrentTopControlsMinHeight;
    private int mCurrentExpectedHeight;
    private boolean mCanAnimateNative;
    private static final int DEFAULT_MIN_HEIGHT = 40;
    private static final int DEFAULT_CONTAINER_HEIGHT = 50;
    private static final int JAVA_HEIGHT = 60;

    @Before
    public void setUp() {
        BrowserControlsStateProvider browserControlsStateProvider =
                new BrowserControlsStateProvider() {
                    @Override
                    public void addObserver(Observer obs) {
                        mCurrentBrowserControlsObserver = obs;
                    }

                    @Override
                    public void removeObserver(Observer obs) {
                        if (mCurrentBrowserControlsObserver == obs) {
                            mCurrentBrowserControlsObserver = null;
                        }
                    }

                    @Override
                    public int getTopControlsHeight() {
                        return mCurrentTopControlsHeight;
                    }

                    @Override
                    public int getTopControlsMinHeight() {
                        return mCurrentTopControlsMinHeight;
                    }

                    @Override
                    public int getTopControlOffset() {
                        return 0;
                    }

                    @Override
                    public int getTopControlsMinHeightOffset() {
                        return 0;
                    }

                    @Override
                    public int getBottomControlsHeight() {
                        return 0;
                    }

                    @Override
                    public int getBottomControlsMinHeight() {
                        return 0;
                    }

                    @Override
                    public int getBottomControlsMinHeightOffset() {
                        return 0;
                    }

                    @Override
                    public boolean shouldAnimateBrowserControlsHeightChanges() {
                        return false;
                    }

                    @Override
                    public int getBottomControlOffset() {
                        return 0;
                    }

                    @Override
                    public float getBrowserControlHiddenRatio() {
                        return 0;
                    }

                    @Override
                    public int getContentOffset() {
                        return 0;
                    }

                    @Override
                    public float getTopVisibleContentOffset() {
                        return 0;
                    }
                };
        Supplier<Boolean> canAnimateNativeSupplier = () -> mCanAnimateNative;
        Supplier<Integer> defaultContainerHeightSupplier = () -> DEFAULT_CONTAINER_HEIGHT;
        Runnable initializeLayout = () -> {};
        mMediator = new ContinuousSearchContainerMediator(browserControlsStateProvider,
                canAnimateNativeSupplier, defaultContainerHeightSupplier, initializeLayout);
        Runnable requestLayout = () -> mMediator.setJavaHeight(JAVA_HEIGHT);
        mModel = new PropertyModel(ContinuousSearchContainerProperties.ALL_KEYS);
        mMediator.onLayoutInitialized(mModel, requestLayout);
    }

    @Test
    public void testShow_WithMinHeight_WithNativeAnimation() {
        triggerShow();

        // State 1. All top controls are offset-ed above the screen.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT + DEFAULT_MIN_HEIGHT, DEFAULT_MIN_HEIGHT,
                true, -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), DEFAULT_MIN_HEIGHT - JAVA_HEIGHT,
                false, View.GONE);

        // State 2. Top controls are half-visible.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT + DEFAULT_MIN_HEIGHT, DEFAULT_MIN_HEIGHT,
                true, -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                ((DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2) + DEFAULT_MIN_HEIGHT, true,
                View.GONE);

        // State 3. Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT + DEFAULT_MIN_HEIGHT, DEFAULT_MIN_HEIGHT,
                true, 0, DEFAULT_CONTAINER_HEIGHT + DEFAULT_MIN_HEIGHT, false, View.VISIBLE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mCurrentBrowserControlsObserver);
    }

    @Test
    public void testShow_WithMinHeight_NoNativeAnimation() {
        triggerShow();

        // State 1. All top controls are offset-ed above the screen.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT + DEFAULT_MIN_HEIGHT, DEFAULT_MIN_HEIGHT,
                false, -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), DEFAULT_MIN_HEIGHT - JAVA_HEIGHT,
                false, View.GONE);

        // State 2. Top controls are half-visible.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT + DEFAULT_MIN_HEIGHT, DEFAULT_MIN_HEIGHT,
                false, -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                ((DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2) + DEFAULT_MIN_HEIGHT, false,
                View.VISIBLE);

        // State 3. Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT + DEFAULT_MIN_HEIGHT, DEFAULT_MIN_HEIGHT,
                false, 0, DEFAULT_CONTAINER_HEIGHT + DEFAULT_MIN_HEIGHT, false, View.VISIBLE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mCurrentBrowserControlsObserver);
    }

    @Test
    public void testShow_NoMinHeight_WithNativeAnimation() {
        triggerShow();

        // State 1. All top controls are offset-ed above the screen.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), -JAVA_HEIGHT, false, View.GONE);

        // State 2. Top controls are half-visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                (DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2, true, View.GONE);

        // State 3. Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, false, View.VISIBLE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mCurrentBrowserControlsObserver);
    }

    @Test
    public void testShow_NoMinHeight_NoNativeAnimation() {
        triggerShow();

        // State 1. All top controls are offset-ed above the screen.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, false,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), -JAVA_HEIGHT, false, View.GONE);

        // State 2. Top controls are half-visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, false,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                (DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2, false, View.VISIBLE);

        // State 3. Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, false,
                0, DEFAULT_CONTAINER_HEIGHT, false, View.VISIBLE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mCurrentBrowserControlsObserver);
    }

    /**
     * (TODO:crbug/1184913) Show/hide animations driven by native are have some unexpected behavior
     * when it comes to interaction between container and toolbar. It's not worth adding more
     * tests for hide until that is fixed.
     */
    @Test
    public void testHide() {
        triggerShow();
        triggerHide();

        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT, 0, true, 0, DEFAULT_CONTAINER_HEIGHT, false, View.GONE);

        Assert.assertNull(
                "Mediator should be not registered as a BrowserControlsStateProvider.Observer.",
                mCurrentBrowserControlsObserver);
    }

    /**
     * Tests that the container is invisible when the tab is obscured.
     */
    @Test
    public void testTabObscured_whileShowing() {
        triggerShow();

        // Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, false, View.VISIBLE);

        mMediator.updateTabObscured(true);
        Assert.assertTrue("Tab obscurity shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
        Assert.assertFalse("Composited view should only be visible while animating.",
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view should be View.INVISIBLE when tab is obscured.",
                View.INVISIBLE,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));

        mMediator.updateTabObscured(false);
        Assert.assertTrue("Tab obscurity shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
        Assert.assertFalse("Composited view should only be visible while animating.",
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view should be View.VISIBLE when tab is not obscured",
                View.VISIBLE,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));
    }

    /**
     * Tests that the container is invisible when the tab is obscured, while the container is
     * animating.
     */
    @Test
    public void testTabObscured_whileAnimating() {
        triggerShow();

        mMediator.updateTabObscured(true);
        // When tab is obscured, Android view should be View.INVISIBLE and composited view should
        // be invisible.

        // State 1. All top controls are offset-ed above the screen.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), -JAVA_HEIGHT, false, View.INVISIBLE);

        // State 2. Top controls are half-visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                (DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2, false, View.INVISIBLE);

        // State 3. Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, false, View.INVISIBLE);

        // ===========================================
        mMediator.updateTabObscured(false);
        // Tab is no longer is obscured. Android view should be View.GONE and composited view should
        // be visible during animation.

        // State 1. All top controls are offset-ed above the screen.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), -JAVA_HEIGHT, false, View.GONE);

        // State 2. Top controls are half-visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                (DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2, true, View.GONE);

        // State 3. Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, false, View.VISIBLE);
    }

    private void triggerShow() {
        CallbackHelper heightObserverCallback = new CallbackHelper();
        mMediator.addHeightObserver(result -> {
            Assert.assertEquals("Height provided by mediator doesn't match java height.",
                    mCurrentExpectedHeight, result.intValue());
            heightObserverCallback.notifyCalled();
        });
        mCurrentExpectedHeight = JAVA_HEIGHT;
        mMediator.show();
        Assert.assertTrue("Mediator should be visible.", mMediator.isVisibleForTesting());
        Assert.assertEquals(
                "Height observer should've been called.", 1, heightObserverCallback.getCallCount());
        Assert.assertEquals("Mediator didn't register BrowserControlsStateProvider.Observer.",
                mMediator, mCurrentBrowserControlsObserver);
    }

    private void triggerHide() {
        mCurrentExpectedHeight = 0;
        mMediator.hide();
        Assert.assertFalse("Mediator should be invisible.", mMediator.isVisibleForTesting());
        Assert.assertEquals(
                "Mediator should still be registered as a BrowserControlsStateProvider.Observer.",
                mMediator, mCurrentBrowserControlsObserver);
    }

    /**
     * Sets browser controls params mocks call to onControlsOffsetChanged. In reality this is done
     * by BrowserControlsStateProvider.
     */
    private void updateBrowserControlParamsAndAssertModel(int topControlsHeight, int minHeight,
            boolean canAnimate, int topOffset, int expectedVerticalOffset,
            boolean expectedCompositedViewVisibility, int expectedAndroidViewVisibility) {
        mCurrentTopControlsHeight = topControlsHeight;
        mCurrentTopControlsMinHeight = minHeight;
        mCanAnimateNative = canAnimate;
        mCurrentBrowserControlsObserver.onControlsOffsetChanged(topOffset, 0, 0, 0, true);
        assertModelVerticalOffset(expectedVerticalOffset);
        assertModelViewVisibility(expectedCompositedViewVisibility, expectedAndroidViewVisibility);
    }

    private void assertModelVerticalOffset(int expectation) {
        Assert.assertEquals("Container vertical offset should be " + expectation, expectation,
                mModel.get(ContinuousSearchContainerProperties.VERTICAL_OFFSET));
    }

    private void assertModelViewVisibility(
            boolean isCompositedViewVisible, int androidViewVisibility) {
        Assert.assertEquals(
                "Composited view should be " + (isCompositedViewVisible ? "" : "in") + "visible.",
                isCompositedViewVisible,
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view visibility should be " + androidViewVisibility,
                androidViewVisibility,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));
    }
}
