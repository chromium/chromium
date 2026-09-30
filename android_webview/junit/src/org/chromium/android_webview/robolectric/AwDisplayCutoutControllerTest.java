// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.os.Build;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.view.WindowInsets;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.android_webview.AwDisplayCutoutController;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** JUnit tests for AwDisplayCutoutController. */
@RunWith(BaseRobolectricTestRunner.class)
public class AwDisplayCutoutControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TAG = "DisplayCutoutTest";
    private static final boolean DEBUG = false;

    @Mock private AwDisplayCutoutController.Delegate mDelegate;
    @Mock private WindowInsets mWindowInsets;
    @Mock private DisplayCutout mDisplayCutout;
    @Mock private ViewTreeObserver mViewTreeObserver;

    private final TestView mView = new TestView();
    private final TestView mAnotherView = new TestView();

    private View.OnApplyWindowInsetsListener mListener;
    private OnPreDrawListener mPreDrawListener;

    private float mDipScale;

    private AwDisplayCutoutController mController;

    /**
     * A View that records calls to requestApplyInsets() and dispatches the test's insets, and
     * returns the test's ViewTreeObserver.
     */
    private class TestView extends View {
        int mRequestApplyInsetsCount;

        TestView() {
            super(ContextUtils.getApplicationContext());
        }

        @Override
        public void setOnApplyWindowInsetsListener(View.OnApplyWindowInsetsListener listener) {
            super.setOnApplyWindowInsetsListener(listener);
            mListener = listener;
        }

        @Override
        public ViewTreeObserver getViewTreeObserver() {
            return mViewTreeObserver;
        }

        @Override
        public void requestApplyInsets() {
            mRequestApplyInsetsCount++;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                if (mListener != null) {
                    mListener.onApplyWindowInsets(this, mWindowInsets);
                }
            } else {
                mController.onApplyWindowInsets(mWindowInsets);
            }
        }
    }

    public AwDisplayCutoutControllerTest() {}

    @Before
    public void setUp() {
        if (DEBUG) Log.i(TAG, "setUp");

        // Set up default values.
        setWindowInsets(new Rect(20, 40, 60, 80));
        mDipScale = 2.0f;

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

        // Set up the delegate.
        when(mDelegate.getDipScale()).thenReturn(mDipScale);
        mController = new AwDisplayCutoutController(mDelegate, mView);
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            Assert.assertNotNull(mListener);
        } else {
            Assert.assertNull(mListener);
        }
    }

    private void setWindowInsets(Rect insets) {
        when(mDisplayCutout.getSafeInsetLeft()).thenReturn(insets.left);
        when(mDisplayCutout.getSafeInsetTop()).thenReturn(insets.top);
        when(mDisplayCutout.getSafeInsetRight()).thenReturn(insets.right);
        when(mDisplayCutout.getSafeInsetBottom()).thenReturn(insets.bottom);
        // Note that prior to Android Q, there is no way to build WindowInsets.
        when(mWindowInsets.getDisplayCutout()).thenReturn(mDisplayCutout);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            when(mWindowInsets.getInsets(anyInt()))
                    .thenAnswer(
                            inv -> {
                                int typeMask = inv.getArgument(0);
                                if ((typeMask & WindowInsetsCompat.Type.ime()) != 0) {
                                    return android.graphics.Insets.of(0, 0, 0, 0);
                                }
                                return android.graphics.Insets.of(
                                        insets.left, insets.top, insets.right, insets.bottom);
                            });
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testOnApplyWindowInsets() {
        mController.onApplyWindowInsets(mWindowInsets);

        verify(mDelegate).getDipScale();
        // Note that DIP of 2.0 is applied, so the values are halved.
        verify(mDelegate).setDisplayCutoutSafeArea(eq(Insets.of(10, 20, 30, 40)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testOnSizeChanged() {
        mController.onSizeChanged();

        // Changing the size of the view should trigger new insets.
        Assert.assertEquals(1, mView.mRequestApplyInsetsCount);
        verify(mDelegate).getDipScale();
        // Note that DIP of 2.0 is applied, so the values are halved.
        verify(mDelegate).setDisplayCutoutSafeArea(eq(Insets.of(10, 20, 30, 40)));
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testOnAttachedToWindow() {
        mController.onAttachedToWindow();

        Assert.assertEquals(1, mView.mRequestApplyInsetsCount);
        verify(mDelegate).getDipScale();
        // Note that DIP of 2.0 is applied, so the values are halved.
        verify(mDelegate).setDisplayCutoutSafeArea(eq(Insets.of(10, 20, 30, 40)));
        Assert.assertNotNull(mPreDrawListener);
    }

    @Test
    @Feature({"AndroidWebView"})
    public void testChangeContainerView_doesNotTriggerOriginalView() {
        // Switching to another container view.
        mController.setCurrentContainerView(mAnotherView);
        mController.onAttachedToWindow();

        Assert.assertEquals(2, mAnotherView.mRequestApplyInsetsCount);
        // Note that mView methods are not triggered.
        Assert.assertEquals(0, mView.mRequestApplyInsetsCount);
    }
}
