// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.view.WindowInsets;

import androidx.core.graphics.Insets;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.android_webview.AwDisplayCutoutController;
import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** JUnit tests for AwDisplayCutoutController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwDisplayCutoutControllerTest {
    private static final String TAG = "DisplayCutoutTest";
    private static final boolean DEBUG = false;

    @Mock private AwDisplayCutoutController.Delegate mDelegate;
    @Mock private WindowInsets mWindowInsets;
    @Mock private DisplayCutout mDisplayCutout;
    @Mock private View mView;
    @Mock private View mAnotherView;
    @Mock private ViewTreeObserver mViewTreeObserver;

    private View.OnApplyWindowInsetsListener mListener;
    private OnPreDrawListener mPreDrawListener;

    private float mDipScale;

    private AwDisplayCutoutController mController;

    public AwDisplayCutoutControllerTest() {
        if (DEBUG) ShadowLog.stream = System.out; // allows logging
    }

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp");
        MockitoAnnotations.initMocks(this);

        // Set up default values.
        setWindowInsets(new Rect(20, 40, 60, 80));
        mDipScale = 2.0f;

        // Set up the view.
        doAnswer(inv -> mListener = (View.OnApplyWindowInsetsListener) inv.getArguments()[0])
                .when(mView)
                .setOnApplyWindowInsetsListener(any(View.OnApplyWindowInsetsListener.class));
        doAnswer(inv -> mListener.onApplyWindowInsets(mView, mWindowInsets))
                .when(mView)
                .requestApplyInsets();
        doAnswer(inv -> mPreDrawListener = (OnPreDrawListener) inv.getArguments()[0])
                .when(mViewTreeObserver)
                .addOnPreDrawListener(any(OnPreDrawListener.class));
        doAnswer(
                        inv -> {
                            Assert.assertEquals(mPreDrawListener, inv.getArguments()[0]);
                            mPreDrawListener = null;
                            return null;
                        })
                .when(mViewTreeObserver)
                .removeOnPreDrawListener(any(OnPreDrawListener.class));

        when(mView.getViewTreeObserver()).thenReturn(mViewTreeObserver);
        when(mAnotherView.getViewTreeObserver()).thenReturn(mViewTreeObserver);

        // Set up the delegate.
        when(mDelegate.getDipScale()).thenReturn(mDipScale);
        mController = new AwDisplayCutoutController(mDelegate, mView);
        verify(mView).setOnApplyWindowInsetsListener(any(View.OnApplyWindowInsetsListener.class));
    }

    private void setWindowInsets(Rect insets) {
        when(mDisplayCutout.getSafeInsetLeft()).thenReturn(insets.left);
        when(mDisplayCutout.getSafeInsetTop()).thenReturn(insets.top);
        when(mDisplayCutout.getSafeInsetRight()).thenReturn(insets.right);
        when(mDisplayCutout.getSafeInsetBottom()).thenReturn(insets.bottom);
        // Note that prior to Android Q, there is no way to build WindowInsets.
        when(mWindowInsets.getDisplayCutout()).thenReturn(mDisplayCutout);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnApplyWindowInsets() {
        mController.onApplyWindowInsets(mWindowInsets);

        verify(mDelegate).getDipScale();
        // Note that DIP of 2.0 is applied, so the values are halved.
        verify(mDelegate).setDisplayCutoutSafeArea(eq(Insets.of(10, 20, 30, 40)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnSizeChanged() {
        mController.onSizeChanged();

        // Changing the size of the view should trigger new insets.
        verify(mView).requestApplyInsets();
        verify(mDelegate).getDipScale();
        // Note that DIP of 2.0 is applied, so the values are halved.
        verify(mDelegate).setDisplayCutoutSafeArea(eq(Insets.of(10, 20, 30, 40)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnAttachedToWindow() {
        mController.onAttachedToWindow();

        verify(mView).requestApplyInsets();
        verify(mDelegate).getDipScale();
        // Note that DIP of 2.0 is applied, so the values are halved.
        verify(mDelegate).setDisplayCutoutSafeArea(eq(Insets.of(10, 20, 30, 40)));
        Assert.assertNotNull(mPreDrawListener);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testChangeContainerView_doesNotTriggerOriginalView() {
        // Switching to another container view.
        mController.setCurrentContainerView(mAnotherView);
        mController.onAttachedToWindow();

        verify(mAnotherView, times(2)).requestApplyInsets();
        // Note that mView methods are not triggered.
        verify(mView, never()).requestApplyInsets();
    }
}
