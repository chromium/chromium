// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.HeightObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link ContinuousSearchContainerMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ContinuousSearchContainerMediatorTest {
    @Mock
    private LayoutStateProvider mLayoutStateProvider;
    private ContinuousSearchContainerMediator mMediator;
    private PropertyModel mModel;
    private BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private int mCurrentTopControlsHeight;
    private int mCurrentTopControlsMinHeight;
    private int mCurrentExpectedHeight;
    private int mCurrentTopOffset;
    private boolean mCanAnimateNative;
    private CallbackHelper mOnHidden;
    private static final int DEFAULT_MIN_HEIGHT = 40;
    private static final int DEFAULT_CONTAINER_HEIGHT = 50;
    private static final int JAVA_HEIGHT = 60;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BrowserControlsStateProvider browserControlsStateProvider =
                new BrowserControlsStateProvider() {
                    @Override
                    public void addObserver(Observer obs) {
                        Assert.assertNull(mBrowserControlsObserver);
                        mBrowserControlsObserver = obs;
                    }

                    @Override
                    public void removeObserver(Observer obs) {
                        if (mBrowserControlsObserver == obs) {
                            mBrowserControlsObserver = null;
                        } else {
                            Assert.fail("Observer removed multiple times.");
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
                        return mCurrentTopOffset;
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
        Callback<Boolean> hideToolbarShadow = (hide) -> {};
        mMediator = new ContinuousSearchContainerMediator(browserControlsStateProvider,
                mLayoutStateProvider, canAnimateNativeSupplier, defaultContainerHeightSupplier,
                initializeLayout, hideToolbarShadow);
        Runnable requestLayout = () -> mMediator.setJavaHeight(JAVA_HEIGHT);
        mMediator.onTopControlsHeightChanged(0, 0); // Ensure this won't crash.
        mModel = new PropertyModel(ContinuousSearchContainerProperties.ALL_KEYS);
        mMediator.onLayoutInitialized(mModel, requestLayout);
        Mockito.when(mLayoutStateProvider.isLayoutVisible(Mockito.eq(LayoutType.BROWSING)))
                .thenReturn(true);
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
                true, 0, DEFAULT_CONTAINER_HEIGHT + DEFAULT_MIN_HEIGHT, true, View.VISIBLE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mBrowserControlsObserver);
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
                mBrowserControlsObserver);
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
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mBrowserControlsObserver);
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
                mBrowserControlsObserver);
    }

    /**
     * (TODO:crbug/1184913) Show/hide animations driven by native are have some unexpected behavior
     * when it comes to interaction between container and toolbar. It's not worth adding more
     * tests for hide until that is fixed.
     */
    @Test
    public void testHide() throws TimeoutException {
        triggerShow();
        triggerHide();

        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT, 0, true, 0, DEFAULT_CONTAINER_HEIGHT, false, View.GONE);
        mOnHidden.waitForFirst();

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mBrowserControlsObserver);
    }

    @Test
    public void testHide_noAnimation() {
        triggerShow();
        Mockito.when(mLayoutStateProvider.isLayoutVisible(Mockito.eq(LayoutType.BROWSING)))
                .thenReturn(false);
        mMediator.addHeightObserver(
                (result, animate)
                        -> Assert.assertFalse("Height change should not be animated", animate));
        triggerHide();

        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT, 0, true, 0, DEFAULT_CONTAINER_HEIGHT, false, View.GONE);

        Assert.assertNotNull(
                "Mediator should be registered as a BrowserControlsStateProvider.Observer.",
                mBrowserControlsObserver);
    }

    /**
     * Tests that the container is invisible when the tab is obscured.
     */
    @Test
    public void testTabObscured_whileShowing() {
        triggerShow();

        // Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        CallbackHelper heightObserverCallback = new CallbackHelper();
        HeightObserver heightObserver = (result, animate) -> {
            Assert.assertEquals(
                    "Height provided by mediator doesn't match java height.", 0, result);
            Assert.assertFalse("Height change shouldn't be animated", animate);
            heightObserverCallback.notifyCalled();
        };
        mMediator.addHeightObserver(heightObserver);
        mMediator.updateTabObscured(true);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertEquals(
                "Height observer should've been called.", 1, heightObserverCallback.getCallCount());
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT, 0, false,
                -DEFAULT_CONTAINER_HEIGHT, -0, false, View.INVISIBLE);

        Assert.assertFalse("Tab obscurity should change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());

        heightObserver = (result, animate) -> {
            Assert.assertEquals(
                    "Height provided by mediator doesn't match java height.", JAVA_HEIGHT, result);
            Assert.assertFalse("Height change shouldn't be animated", animate);
            heightObserverCallback.notifyCalled();
        };
        mMediator.addHeightObserver(heightObserver);
        mMediator.updateTabObscured(false);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertEquals(
                "Height observer should've been called.", 2, heightObserverCallback.getCallCount());
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        Assert.assertTrue("Tab obscurity should change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
    }

    /**
     * Tests that multiple tab obscures are handled correctly.
     */
    @Test
    public void testForceShowHide() {
        triggerShow();

        // Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        // Unhide while unhidden should be a no-op.
        mMediator.showContainer(TokenHolder.INVALID_TOKEN);

        int firstToken = mMediator.hideContainer();
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT, 0, false,
                -DEFAULT_CONTAINER_HEIGHT, -0, false, View.INVISIBLE);

        Assert.assertFalse("Hide container should change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());

        // Hide again while already hidden.
        int secondToken = mMediator.hideContainer();

        // First unhide should be a no-op.
        mMediator.showContainer(secondToken);
        Assert.assertFalse("Nested force hides shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());

        mMediator.showContainer(firstToken);
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        Assert.assertTrue("Show container should change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
    }

    /**
     * Tests that the container is invisible when the tab is obscured, while the container is
     * animating.
     */
    @Test
    public void testTabObscured_whileAnimating() {
        // Start the animation.
        triggerShow();

        // Pretend the animation is half started.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT) / 2,
                (DEFAULT_CONTAINER_HEIGHT - JAVA_HEIGHT) / 2, true, View.GONE);

        // Obscure the tab. This will cancel the current animation and set the height to 0.
        CallbackHelper heightObserverCallback = new CallbackHelper();
        HeightObserver heightObserver = (result, animate) -> {
            Assert.assertEquals(
                    "Height provided by mediator doesn't match java height.", 0, result);
            Assert.assertFalse("Height change shouldn't be animated", animate);
            heightObserverCallback.notifyCalled();
        };
        mMediator.addHeightObserver(heightObserver);
        mMediator.updateTabObscured(true);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertEquals(
                "Height observer should've been called.", 1, heightObserverCallback.getCallCount());
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT, 0, false,
                -DEFAULT_CONTAINER_HEIGHT, -0, false, View.INVISIBLE);

        Assert.assertFalse("Tab obscurity should change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());

        heightObserver = (result, animate) -> {
            Assert.assertEquals(
                    "Height provided by mediator doesn't match java height.", JAVA_HEIGHT, result);
            Assert.assertFalse("Height change shouldn't be animated", animate);
            heightObserverCallback.notifyCalled();
        };
        mMediator.addHeightObserver(heightObserver);
        mMediator.updateTabObscured(false);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertEquals(
                "Height observer should've been called.", 2, heightObserverCallback.getCallCount());
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        Assert.assertTrue("Tab obscurity should change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());

        // Now do this opposite for hide.
        triggerHide();

        // TODO(crbug/1184913): Show/hide animations driven by native and mid-state animation is
        // difficult to simulate due to some existing bugs. Update this when that behavior is fixed.
        updateBrowserControlParamsAndAssertModel(
                DEFAULT_CONTAINER_HEIGHT, 0, true, 0, DEFAULT_CONTAINER_HEIGHT, false, View.GONE);

        // This should no-op as the view is no longer visible.
        heightObserver = (result, animate) -> {
            heightObserverCallback.notifyCalled();
        };
        mMediator.addHeightObserver(heightObserver);
        mMediator.updateTabObscured(true);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertEquals("Height observer shouldn't have been called.", 2,
                heightObserverCallback.getCallCount());

        // This should no-op as the view is no longer visible.
        mMediator.addHeightObserver(heightObserver);
        mMediator.updateTabObscured(false);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertEquals("Height observer shouldn't have been called.", 2,
                heightObserverCallback.getCallCount());
    }

    /**
     * Tests that Android view visibility is updated correctly.
     */
    @Test
    public void testAndroidViewVisibilityChanged() {
        triggerShow();

        // Top controls are fully visible.
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true, 0,
                DEFAULT_CONTAINER_HEIGHT, true, View.VISIBLE);

        mMediator.onAndroidVisibilityChanged(View.INVISIBLE);

        Assert.assertTrue("Android View visibility shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
        Assert.assertTrue("Composited view should be visible.",
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view should be invisible.", View.INVISIBLE,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));

        mMediator.onAndroidVisibilityChanged(View.VISIBLE);

        Assert.assertTrue("Android View visibility shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
        Assert.assertTrue("Composited view should be visible.",
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view should be visible.", View.VISIBLE,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));

        // Hide the UI.
        triggerHide();
        updateBrowserControlParamsAndAssertModel(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT, 0, true,
                -(DEFAULT_CONTAINER_HEIGHT + JAVA_HEIGHT), -JAVA_HEIGHT, false, View.GONE);

        mMediator.onAndroidVisibilityChanged(View.INVISIBLE);

        Assert.assertFalse("Android View visibility shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
        Assert.assertFalse("Composited view should be invisible.",
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view should be invisible.", View.INVISIBLE,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));

        mMediator.onAndroidVisibilityChanged(View.VISIBLE);

        Assert.assertFalse("Android View visibility shouldn't change mMediator.mIsVisible.",
                mMediator.isVisibleForTesting());
        Assert.assertFalse("Composited view should be invisible.",
                mModel.get(ContinuousSearchContainerProperties.COMPOSITED_VIEW_VISIBLE));
        Assert.assertEquals("Android view should be invisible.", View.INVISIBLE,
                mModel.get(ContinuousSearchContainerProperties.ANDROID_VIEW_VISIBILITY));
    }

    private void triggerShow() {
        CallbackHelper heightObserverCallback = new CallbackHelper();
        HeightObserver heightObserver = (result, animate) -> {
            Assert.assertEquals("Height provided by mediator doesn't match java height.",
                    mCurrentExpectedHeight, result);
            Assert.assertFalse("Height change shouldn't be animated", animate);
            heightObserverCallback.notifyCalled();
        };
        mMediator.addHeightObserver(heightObserver);
        mCurrentExpectedHeight = JAVA_HEIGHT;
        mMediator.show(null);
        mMediator.removeHeightObserver(heightObserver);
        Assert.assertTrue("Mediator should be visible.", mMediator.isVisibleForTesting());
        Assert.assertEquals(
                "Height observer should've been called.", 1, heightObserverCallback.getCallCount());
        Assert.assertEquals("Mediator didn't register BrowserControlsStateProvider.Observer.",
                mMediator, mBrowserControlsObserver);
    }

    private void triggerHide() {
        mCurrentExpectedHeight = 0;
        mOnHidden = new CallbackHelper();
        mMediator.hide(mOnHidden::notifyCalled);
        Assert.assertFalse("Mediator should be invisible.", mMediator.isVisibleForTesting());
        Assert.assertEquals(
                "Mediator should still be registered as a BrowserControlsStateProvider.Observer.",
                mMediator, mBrowserControlsObserver);
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
        mCurrentTopOffset = topOffset;
        mCanAnimateNative = canAnimate;
        mBrowserControlsObserver.onTopControlsHeightChanged(topControlsHeight, minHeight);
        mBrowserControlsObserver.onControlsOffsetChanged(topOffset, 0, 0, 0, true);
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
