// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Matrix;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.android_webview.AwDisplayModeController;
import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.DisplayMode;

/** JUnit tests for AwDisplayModeController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwDisplayModeControllerTest {
    private static final String TAG = "DisplayModeTest";
    private static final boolean DEBUG = false;

    private InOrder mInOrder;
    private Context mContext;

    @Mock private AwDisplayModeController.Delegate mDelegate;
    @Mock private View mView;
    @Mock private View mAnotherView;

    @Mock private ViewGroup mParentView;
    @Mock private ViewGroup mRootView;

    private View.OnApplyWindowInsetsListener mListener;
    private int[] mLocationOnScreen = {0, 0};
    private int mViewWidth;
    private int mViewHeight;

    private Matrix mGlobalTransformMatrix;

    private float mDipScale;
    private int mDisplayWidth;
    private int mDisplayHeight;

    private AwDisplayModeController mController;

    public AwDisplayModeControllerTest() {
        if (DEBUG) ShadowLog.stream = System.out; // allows logging
    }

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp");
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;

        // Set up default values.
        mViewWidth = 300;
        mViewHeight = 400;
        mDisplayWidth = 300;
        mDisplayHeight = 400;
        mGlobalTransformMatrix = new Matrix(); // identity matrix

        // Set up the view.
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) throws Throwable {
                                int[] loc = (int[]) invocation.getArguments()[0];
                                loc[0] = mLocationOnScreen[0];
                                loc[1] = mLocationOnScreen[1];
                                return null;
                            }
                        })
                .when(mView)
                .getLocationOnScreen(any(int[].class));

        when(mView.getMeasuredWidth()).thenReturn(mViewWidth);
        when(mView.getMeasuredHeight()).thenReturn(mViewHeight);
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) throws Throwable {
                                Matrix matrix = (Matrix) invocation.getArguments()[0];
                                matrix.set(mGlobalTransformMatrix);
                                return null;
                            }
                        })
                .when(mView)
                .transformMatrixToGlobal(any(Matrix.class));

        // Set up the root view.
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) throws Throwable {
                                int[] loc = (int[]) invocation.getArguments()[0];
                                loc[0] = mLocationOnScreen[0];
                                loc[1] = mLocationOnScreen[1];
                                return null;
                            }
                        })
                .when(mRootView)
                .getLocationOnScreen(any(int[].class));
        when(mRootView.getMeasuredWidth()).thenReturn(mViewWidth);
        when(mRootView.getMeasuredHeight()).thenReturn(mViewHeight);
        when(mView.getRootView()).thenReturn(mRootView);

        // Set up the delegate.
        when(mDelegate.getDisplayWidth()).thenReturn(mDisplayWidth);
        when(mDelegate.getDisplayHeight()).thenReturn(mDisplayHeight);

        mInOrder = inOrder(mDelegate, mView, mAnotherView);

        mController = new AwDisplayModeController(mDelegate, mView);

        mInOrder.verifyNoMoreInteractions();
    }

    @After
    public void tearDown() {
        if (DEBUG) Log.i(TAG, "tearDown");
        mInOrder.verifyNoMoreInteractions();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFullscreen() {
        Assert.assertEquals(DisplayMode.FULLSCREEN, mController.getDisplayMode());

        mInOrder.verify(mView).getLocationOnScreen(any(int[].class));
        mInOrder.verify(mView).getMeasuredWidth();
        mInOrder.verify(mView).getMeasuredHeight();
        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
        mInOrder.verify(mView).transformMatrixToGlobal(any(Matrix.class));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotFullscreen_NotOccupyingFullDisplay() {
        // View is not occupying the entire display, so no insets applied.
        when(mView.getMeasuredHeight()).thenReturn(mDisplayHeight / 2);

        Assert.assertEquals(DisplayMode.BROWSER, mController.getDisplayMode());

        mInOrder.verify(mView).getLocationOnScreen(any(int[].class));
        mInOrder.verify(mView).getMeasuredWidth();
        mInOrder.verify(mView).getMeasuredHeight();
        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotFullscreen_NotOccupyingFullWindow() {
        // View is not occupying the entire window, so no insets applied.
        when(mRootView.getMeasuredHeight()).thenReturn(mViewHeight / 2);

        Assert.assertEquals(DisplayMode.BROWSER, mController.getDisplayMode());

        mInOrder.verify(mView).getLocationOnScreen(any(int[].class));
        mInOrder.verify(mView).getMeasuredWidth();
        mInOrder.verify(mView).getMeasuredHeight();
        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNotFullscreen_ParentLayoutRotated() {
        mGlobalTransformMatrix.postRotate(30.0f);

        Assert.assertEquals(DisplayMode.BROWSER, mController.getDisplayMode());

        mInOrder.verify(mView).getLocationOnScreen(any(int[].class));
        mInOrder.verify(mView).getMeasuredWidth();
        mInOrder.verify(mView).getMeasuredHeight();
        mInOrder.verify(mDelegate).getDisplayWidth();
        mInOrder.verify(mDelegate).getDisplayHeight();
        mInOrder.verify(mView).transformMatrixToGlobal(any(Matrix.class));
    }
}
