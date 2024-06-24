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
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link StatusIndicatorMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StatusIndicatorMediatorTest {

    @Mock BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock TabObscuringHandler mTabObscuringHandler;
    @Mock View mStatusIndicatorView;
    @Mock StatusIndicatorCoordinator.StatusIndicatorObserver mObserver;
    @Mock Runnable mRegisterResource;
    @Mock Runnable mUnregisterResource;
    @Mock Supplier<Boolean> mCanAnimateNativeBrowserControls;
    @Mock Callback<Runnable> mInvalidateCompositorView;
    @Mock Runnable mRequestLayout;

    private PropertyModel mModel;
    private StatusIndicatorMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
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
        mMediator.onControlsOffsetChanged(0, 70, 0, 0, false, false);

        // Now, hide it. Listener shouldn't be removed.
        mMediator.updateVisibilityForTesting(true);
        verify(mBrowserControlsStateProvider, never()).removeObserver(mMediator);

        // Once the hiding animation is done...
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false, false);
        // The listener should be removed.
        verify(mBrowserControlsStateProvider).removeObserver(mMediator);
    }

    @Test
    public void testHeightChangeToZeroMakesAndroidViewGone() {
        // Show the status indicator.
        setViewHeight(70);
        mMediator.onLayoutChange(mStatusIndicatorView, 0, 0, 0, 0, 0, 0, 0, 0);
        mMediator.onControlsOffsetChanged(0, 70, 0, 0, false, false);
        // The Android view should be visible at this point.
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        // Now hide it.
        mMediator.updateVisibilityForTesting(true);
        // The hiding animation...
        mMediator.onControlsOffsetChanged(0, 30, 0, 0, false, false);
        // Android view will be gone once the animation starts.
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false, false);
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
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false, false);
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertFalse(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Status indicator is partially showing.
        mMediator.onControlsOffsetChanged(0, 10, 0, 0, false, false);
        assertEquals(View.INVISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Status indicator is fully showing, 20px.
        mMediator.onControlsOffsetChanged(0, 20, 0, 0, false, false);
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        assertTrue(mModel.get(StatusIndicatorProperties.COMPOSITED_VIEW_VISIBLE));

        // Hide again.
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false, false);
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
        mMediator.onControlsOffsetChanged(0, 70, 0, 0, false, false);
        // The Android view should be visible at this point.
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        // Now hide it.
        mMediator.updateVisibilityForTesting(true);
        // The hiding animation...
        mMediator.onControlsOffsetChanged(0, 30, 0, 0, false, false);
        // Android view will be VISIBLE during the animation.
        assertEquals(View.VISIBLE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
        mMediator.onControlsOffsetChanged(0, 0, 0, 0, false, false);
        // The view will be GONE once the animation ends and the indicator is completely out of
        // screen bounds.
        assertEquals(View.GONE, mModel.get(StatusIndicatorProperties.ANDROID_VIEW_VISIBILITY));
    }

    private void setViewHeight(int height) {
        when(mStatusIndicatorView.getHeight()).thenReturn(height);
    }
}
