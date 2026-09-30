// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.android_webview.AwDisplayModeController;
import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.DisplayMode;

/** JUnit tests for AwDisplayModeController. */
@RunWith(BaseRobolectricTestRunner.class)
public class AwDisplayModeControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TAG = "DisplayModeTest";
    private static final boolean DEBUG = false;

    private InOrder mInOrder;
    private Context mContext;

    @Mock private AwDisplayModeController.Delegate mDelegate;

    private View mView;
    private FrameLayout mRootView;

    private int mViewWidth;
    private int mViewHeight;

    private int mDisplayWidth;
    private int mDisplayHeight;

    private AwDisplayModeController mController;

    public AwDisplayModeControllerTest() {}

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp");
        mContext = RuntimeEnvironment.application;

        // Set up default values.
        mViewWidth = 300;
        mViewHeight = 400;
        mDisplayWidth = 300;
        mDisplayHeight = 400;

        // Set up the view and the root view. Neither is attached to a window, so their locations
        // on screen are (0, 0).
        mRootView = new FrameLayout(mContext);
        mView = new View(mContext);
        mRootView.addView(mView, new FrameLayout.LayoutParams(mViewWidth, mViewHeight));
        measure(mRootView, mViewWidth, mViewHeight);

        // Set up the delegate.
        when(mDelegate.getDisplayWidth()).thenReturn(mDisplayWidth);
        when(mDelegate.getDisplayHeight()).thenReturn(mDisplayHeight);

        mInOrder = inOrder(mDelegate);

        mController = new AwDisplayModeController(mDelegate, mView);

        mInOrder.verifyNoMoreInteractions();
    }

    @After
    public void tearDown() {
        if (DEBUG) Log.i(TAG, "tearDown");
        mInOrder.verifyNoMoreInteractions();
    }

    private static void measure(View view, int width, int height) {
        view.measure(
                MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testFullscreen() {
        Assert.assertEquals(DisplayMode.FULLSCREEN, mController.getDisplayMode());

        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testNotFullscreen_NotOccupyingFullDisplay() {
        // View is not occupying the entire display, so no insets applied.
        measure(mView, mViewWidth, mDisplayHeight / 2);

        Assert.assertEquals(DisplayMode.BROWSER, mController.getDisplayMode());

        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testNotFullscreen_NotOccupyingFullWindow() {
        // View is not occupying the entire window, so no insets applied.
        measure(mRootView, mViewWidth, mViewHeight / 2);
        // The view has fixed LayoutParams, so it keeps its size.
        Assert.assertEquals(mViewHeight, mView.getMeasuredHeight());

        Assert.assertEquals(DisplayMode.BROWSER, mController.getDisplayMode());

        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testNotFullscreen_ParentLayoutRotated() {
        mRootView.setRotation(30.0f);

        Assert.assertEquals(DisplayMode.BROWSER, mController.getDisplayMode());

        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
    }
}
