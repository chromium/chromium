// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.os.Build;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Unit tests for {@link StatusIndicatorMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StatusIndicatorMediatorTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock TopControlsStacker mTopControlsStacker;
    @Mock BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock TabObscuringHandler mTabObscuringHandler;
    @Mock StatusIndicatorCoordinator.StatusIndicatorObserver mObserver;
    @Mock Runnable mRegisterResource;
    @Mock Runnable mUnregisterResource;
    @Mock Supplier<Boolean> mCanAnimateNativeBrowserControls;
    @Mock Callback<Runnable> mInvalidateCompositorView;
    @Mock Runnable mRequestLayout;

    private final View mStatusIndicatorView = new View(ContextUtils.getApplicationContext());
    private PropertyModel mModel;
    private StatusIndicatorMediator mMediator;

    @Before
    public void setUp() {
        doNothing().when(mRegisterResource).run();
        doNothing().when(mUnregisterResource).run();
        when(mCanAnimateNativeBrowserControls.get()).thenReturn(true);
        doNothing().when(mInvalidateCompositorView).onResult(any(Runnable.class));
        doNothing().when(mRequestLayout).run();
        mModel =
                new PropertyModel.Builder(StatusIndicatorProperties.ALL_KEYS)
                        .with(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY, View.GONE)
                        .with(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE, false)
                        .build();
        mMediator =
                new StatusIndicatorMediator(
                        mTopControlsStacker,
                        mBrowserControlsStateProvider,
                        mTabObscuringHandler,
                        () -> Color.WHITE,
                        mCanAnimateNativeBrowserControls);
        mMediator.initialize(
                mModel,
                mRegisterResource,
                mUnregisterResource,
                mInvalidateCompositorView,
                mRequestLayout);
    }

    @Test
    public void testHeightChangeAddsListener() {
        // After layout
        setViewHeight(70);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        verify(mBrowserControlsStateProvider).addObserver(mMediator);
    }

    @Test
    public void testHeightChangeNotifiesObservers() {
        // Add an observer.
        mMediator.addObserver(mObserver);
        // After layout
        setViewHeight(70);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        verify(mObserver).onStatusIndicatorHeightChanged(70);
        mMediator.removeObserver(mObserver);
    }

    @Test
    public void testHeightChangeDoesNotRemoveListenerImmediately() {
        // Show the status indicator.
        setViewHeight(70);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        mMediator.onControlsOffsetChanged(0, 70, true, 0, 0, false, false, false);

        // Now, hide it. Listener shouldn't be removed.
        mMediator.updateVisibilityForTesting(true);
        verify(mBrowserControlsStateProvider, never()).removeObserver(mMediator);

        // Once the hiding animation is done...
        mMediator.onControlsOffsetChanged(0, 0, true, 0, 0, false, false, false);
        // The listener should be removed.
        verify(mBrowserControlsStateProvider).removeObserver(mMediator);
    }

    @Test
    public void testHeightChangeToZeroMakesAndroidViewGone() {
        // Show the status indicator.
        setViewHeight(70);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        mMediator.onControlsOffsetChanged(0, 70, true, 0, 0, false, false, false);
        // The Android view should be visible at this point.
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        // Now hide it.
        mMediator.updateVisibilityForTesting(true);
        // The hiding animation...
        mMediator.onControlsOffsetChanged(0, 30, true, 0, 0, false, false, false);
        // Android view will be gone once the animation starts.
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        mMediator.onControlsOffsetChanged(0, 0, true, 0, 0, false, false, false);
        // Shouldn't make the Android view invisible. It should stay gone.
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
    }

    @Test
    public void testOffsetChangeUpdatesVisibility() {
        // Initially, the Android view should be GONE.
        setViewHeight(20);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        // Assume the status indicator is completely hidden.
        mMediator.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Status indicator is partially showing.
        mMediator.onControlsOffsetChanged(0, 10, true, 0, 0, false, false, false);
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Status indicator is fully showing, 20px.
        mMediator.onControlsOffsetChanged(0, 20, true, 0, 0, false, false, false);
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Hide again.
        mMediator.onControlsOffsetChanged(0, 0, true, 0, 0, false, false, false);
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
    }

    @Test
    public void testHeightChangeToZeroKeepsAndroidViewVisibleIfCannotAnimateNativeControls() {
        // Assume we can't animate native controls.
        when(mCanAnimateNativeBrowserControls.get()).thenReturn(false);
        // Show the status indicator.
        setViewHeight(70);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        mMediator.onControlsOffsetChanged(0, 70, true, 0, 0, false, false, false);
        // The Android view should be visible at this point.
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        // Now hide it.
        mMediator.updateVisibilityForTesting(true);
        // The hiding animation...
        mMediator.onControlsOffsetChanged(0, 30, true, 0, 0, false, false, false);
        // Android view will be VISIBLE during the animation.
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        mMediator.onControlsOffsetChanged(0, 0, true, 0, 0, false, false, false);
        // The view will be GONE once the animation ends and the indicator is completely out of
        // screen bounds.
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    // EdgeToEdgeUtils.isTopEdgeToEdgeEnabled() requires Android R+ (API 30).
    @Config(sdk = Build.VERSION_CODES.R)
    public void testOnOffsetChanged_RequeriesHeightFromLayerToTopAcrossFullscreenEnterExit() {
        setViewHeight(20);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);

        // Initially TOP_SCALP (30px) is above STATUS_INDICATOR; clamp if minHeightOffset < 30.
        when(mTopControlsStacker.getHeightFromLayerToTop(TopControlType.STATUS_INDICATOR))
                .thenReturn(30);
        mMediator.onControlsOffsetChanged(0, 15, false, 0, 0, false, false, false);
        assertEquals(30, mModel.get(StatusIndicatorProperties.TOP_OFFSET));
        assertEquals(0, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Indicator partially slides in (10px of 20px) below 30px TOP_SCALP -> minHeightOffset=40.
        mMediator.onControlsOffsetChanged(0, 40, true, 0, 0, false, false, false);
        assertEquals(30, mModel.get(StatusIndicatorProperties.TOP_OFFSET));
        assertEquals(10, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Indicator slides in (20px) below 30px TOP_SCALP -> total minHeightOffset = 50.
        mMediator.onControlsOffsetChanged(0, 50, true, 0, 0, false, false, false);
        assertEquals(30, mModel.get(StatusIndicatorProperties.TOP_OFFSET));
        assertEquals(20, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Enter HTML fullscreen: TOP_SCALP height becomes 0.
        when(mTopControlsStacker.getHeightFromLayerToTop(TopControlType.STATUS_INDICATOR))
                .thenReturn(0);
        mMediator.onControlsOffsetChanged(0, 20, true, 0, 0, false, false, false);
        assertEquals(0, mModel.get(StatusIndicatorProperties.TOP_OFFSET));
        assertEquals(20, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Exit HTML fullscreen: TOP_SCALP height returns to 30.
        when(mTopControlsStacker.getHeightFromLayerToTop(TopControlType.STATUS_INDICATOR))
                .thenReturn(30);
        mMediator.onControlsOffsetChanged(0, 50, true, 0, 0, false, false, false);
        assertEquals(30, mModel.get(StatusIndicatorProperties.TOP_OFFSET));
        assertEquals(20, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Hide indicator while TOP_SCALP (30px) remains active.
        mMediator.updateVisibilityForTesting(true);
        mMediator.onControlsOffsetChanged(0, 40, true, 0, 0, false, false, false);
        assertEquals(10, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        verify(mBrowserControlsStateProvider, never()).removeObserver(mMediator);
        verify(mUnregisterResource, never()).run();

        // doneHiding triggers once topControlsMinHeightOffset reaches offsetToTop (30px).
        mMediator.onControlsOffsetChanged(0, 30, true, 0, 0, false, false, false);
        assertEquals(0, mModel.get(StatusIndicatorProperties.CURRENT_VISIBLE_HEIGHT));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        verify(mBrowserControlsStateProvider).removeObserver(mMediator);
        verify(mUnregisterResource).run();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    // EdgeToEdgeUtils.isTopEdgeToEdgeEnabled() requires Android R+ (API 30).
    @Config(sdk = Build.VERSION_CODES.R)
    public void testCannotAnimateNativeControls_WithTopScalp_StaysInvisibleUntilVisibleHeight() {
        when(mCanAnimateNativeBrowserControls.get()).thenReturn(false);
        when(mTopControlsStacker.getHeightFromLayerToTop(TopControlType.STATUS_INDICATOR))
                .thenReturn(30);
        setViewHeight(20);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);

        // Before indicator slides below TOP_SCALP (minHeightOffset == 30), Android view must stay
        // INVISIBLE rather than rendering VISIBLE inside the status bar [10, 30).
        mMediator.onControlsOffsetChanged(0, 30, false, 0, 0, false, false, false);
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Once indicatorVisibleHeight > 0, Android view is VISIBLE during Java animation.
        mMediator.onControlsOffsetChanged(0, 40, true, 0, 0, false, false, false);
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));

        // Hide back to TOP_SCALP (30px): Android view becomes GONE and doneHiding cleans up.
        mMediator.updateVisibilityForTesting(true);
        mMediator.onControlsOffsetChanged(0, 30, true, 0, 0, false, false, false);
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        verify(mBrowserControlsStateProvider).removeObserver(mMediator);
    }

    private void setViewHeight(int height) {
        mStatusIndicatorView.layout(0, 0, 0, height);
    }
}
