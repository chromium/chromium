// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link PartialCustomTabHandleStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES,
        ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabHeightStrategyTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    // Pixel 3 XL metrics
    private static final int DEVICE_HEIGHT = 2960;
    private static final int DEVICE_WIDTH = 1440;

    private static final int NAVBAR_HEIGHT = 160;
    private static final int MAX_INIT_POS = DEVICE_HEIGHT / 2 - NAVBAR_HEIGHT;

    @Mock
    private Activity mActivity;
    @Mock
    private Window mWindow;
    @Mock
    private WindowManager mWindowManager;
    @Mock
    private Resources mResources;
    @Mock
    private Configuration mConfiguration;
    private WindowManager.LayoutParams mAttributes;
    @Mock
    private ViewStub mHandleViewStub;
    @Mock
    private ImageView mHandleView;
    @Mock
    private View mDecorView;
    @Mock
    private View mRootView;
    @Mock
    private Display mDisplay;
    @Mock
    private PartialCustomTabHeightStrategy.OnResizedCallback mOnResizedCallback;
    @Mock
    private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private LinearLayout mNavbar;
    @Mock
    private ViewPropertyAnimator mViewAnimator;
    @Mock
    private ImageView mSpinnerView;
    @Mock
    private CircularProgressDrawable mSpinner;
    @Mock
    private View mToolbarView;
    @Mock
    private View mToolbarCoordinator;
    @Mock
    private FrameLayout mParentView;
    @Mock
    private ViewGroup mCoordinatorLayout;

    private List<WindowManager.LayoutParams> mAttributeResults;
    private DisplayMetrics mRealMetrics;
    private Point mDisplaySize;
    private ObservableSupplierImpl<FrameLayout> mParentViewSupplier =
            new ObservableSupplierImpl<>();
    private Callback<Integer> mBottomInsetCallback = inset -> {};
    private FrameLayout.LayoutParams mLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getWindowManager()).thenReturn(mWindowManager);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view_stub)).thenReturn(mHandleViewStub);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view)).thenReturn(mHandleView);
        when(mHandleView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mToolbarCoordinator.getLayoutParams()).thenReturn(mLayoutParams);
        mAttributes = new WindowManager.LayoutParams();
        when(mWindow.getAttributes()).thenReturn(mAttributes);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mDecorView.getRootView()).thenReturn(mRootView);
        when(mRootView.getLayoutParams()).thenReturn(mAttributes);
        when(mWindowManager.getDefaultDisplay()).thenReturn(mDisplay);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        when(mNavbar.getLayoutParams()).thenReturn(mLayoutParams);
        when(mNavbar.animate()).thenReturn(mViewAnimator);
        when(mViewAnimator.alpha(anyFloat())).thenReturn(mViewAnimator);
        when(mViewAnimator.setDuration(anyLong())).thenReturn(mViewAnimator);
        when(mViewAnimator.setListener(anyObject())).thenReturn(mViewAnimator);
        when(mSpinnerView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mSpinnerView.getParent()).thenReturn(mParentView);
        when(mSpinnerView.animate()).thenReturn(mViewAnimator);
        when(mParentView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mParentView.getParent()).thenReturn(mCoordinatorLayout);
        when(mCoordinatorLayout.getLayoutParams()).thenReturn(mLayoutParams);

        mParentViewSupplier.set(mParentView);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;

        mAttributeResults = new ArrayList<>();
        doAnswer(invocation -> {
            WindowManager.LayoutParams attributes = new WindowManager.LayoutParams();
            attributes.copyFrom((WindowManager.LayoutParams) invocation.getArgument(0));
            mAttributeResults.add(attributes);
            return null;
        })
                .when(mWindow)
                .setAttributes(any(WindowManager.LayoutParams.class));

        mRealMetrics = new DisplayMetrics();
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        doAnswer(invocation -> {
            DisplayMetrics displayMetrics = invocation.getArgument(0);
            displayMetrics.setTo(mRealMetrics);
            return null;
        })
                .when(mDisplay)
                .getRealMetrics(any(DisplayMetrics.class));

        mDisplaySize = new Point();
        mDisplaySize.x = DEVICE_WIDTH;
        mDisplaySize.y = DEVICE_HEIGHT - NAVBAR_HEIGHT;
        doAnswer(invocation -> {
            Point point = invocation.getArgument(0);
            point.x = mDisplaySize.x;
            point.y = mDisplaySize.y;
            return null;
        })
                .when(mDisplay)
                .getSize(any(Point.class));
    }

    @Test
    public void create_heightIsCappedToHalfOfDeviceHeight() {
        new PartialCustomTabHeightStrategy(mActivity, mParentViewSupplier, 500,
                mMultiWindowModeStateDispatcher, null, null, mOnResizedCallback,
                mActivityLifecycleDispatcher);

        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertEquals(MAX_INIT_POS, mAttributeResults.get(0).y);
    }

    @Test
    public void create_largeInitialHeight() {
        new PartialCustomTabHeightStrategy(mActivity, mParentViewSupplier, 5000,
                mMultiWindowModeStateDispatcher, null, null, mOnResizedCallback,
                mActivityLifecycleDispatcher);

        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertEquals(0, mAttributeResults.get(0).y);
    }

    @Test
    public void create_heightIsCappedToDeviceHeight() {
        new PartialCustomTabHeightStrategy(mActivity, mParentViewSupplier, DEVICE_HEIGHT + 100,
                mMultiWindowModeStateDispatcher, null, null, mOnResizedCallback,
                mActivityLifecycleDispatcher);

        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertEquals(0, mAttributeResults.get(0).y);
    }

    @Test
    public void create_landscapeOrientation() {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        mDisplaySize.x = DEVICE_HEIGHT - NAVBAR_HEIGHT;
        mDisplaySize.y = DEVICE_WIDTH;
        new PartialCustomTabHeightStrategy(mActivity, mParentViewSupplier, 800,
                mMultiWindowModeStateDispatcher, null, null, mOnResizedCallback,
                mActivityLifecycleDispatcher);

        verifyWindowFlagsSet();

        // Full height when in landscape mode.
        assertEquals(1, mAttributeResults.size());
        assertEquals(0, mAttributeResults.get(0).y);
    }

    @Test
    public void moveUp() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 500, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);
        strategy.setMockViewForTesting(mNavbar, mSpinnerView, mSpinner, mToolbarView,
                mToolbarCoordinator, mCoordinatorLayout);

        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertEquals(MAX_INIT_POS, mAttributeResults.get(0).y);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        // action down
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0));
        // action move
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1450, 0));
        // action move again
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1400, 0));
        // action up
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, DEVICE_WIDTH / 2, 1400, 0));

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();

        final int length = mAttributeResults.size();
        assertTrue(length > 1);
        // Move to cover the whole screen.
        assertEquals(0, mAttributeResults.get(length - 1).y);
    }

    @Test
    public void moveUp_landscapeOrientationUnresizable() {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 800, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        // action down
        assertFalse(handleStrategy.onInterceptTouchEvent(
                MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0)));
    }

    @Test
    public void moveUp_multiwindowModeUnresizable() {
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(true);
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 800, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        // action down
        assertFalse(handleStrategy.onInterceptTouchEvent(
                MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0)));
    }

    @Test
    public void rotateToLandescapeUnresizable() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 800, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);
        strategy.setMockViewForTesting(mNavbar, mSpinnerView, mSpinner, mToolbarView,
                mToolbarCoordinator, mCoordinatorLayout);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);

        // action down
        assertFalse(handleStrategy.onInterceptTouchEvent(
                MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0)));
    }

    @Test
    public void rotateToLandescapeHideCustomNavbar() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 800, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);
        strategy.setMockViewForTesting(mNavbar, mSpinnerView, mSpinner, mToolbarView,
                mToolbarCoordinator, mCoordinatorLayout);

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        mDisplaySize.x = DEVICE_HEIGHT - NAVBAR_HEIGHT;
        mDisplaySize.y = DEVICE_WIDTH;

        strategy.onConfigurationChanged(mConfiguration);

        assertEquals(0, strategy.getNavbarHeightForTesting());
        verify(mNavbar, times(1)).setVisibility(View.GONE);
    }

    @Test
    public void enterMultiwindowModeUnresizable() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 800, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        strategy.onMultiWindowModeChanged(true);

        // action down
        assertFalse(handleStrategy.onInterceptTouchEvent(
                MotionEvent.obtain(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(),
                        MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0)));
    }

    @Test
    public void moveUpThenDown() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 500, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);
        strategy.setMockViewForTesting(mNavbar, mSpinnerView, mSpinner, mToolbarView,
                mToolbarCoordinator, mCoordinatorLayout);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertEquals(MAX_INIT_POS, mAttributeResults.get(0).y);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        // action down
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0));
        // action move
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1450, 0));
        // action move again
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1600, 0));
        // action up
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, DEVICE_WIDTH / 2, 1600, 0));

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();

        final int length = mAttributeResults.size();
        assertTrue(length > 1);
        // Back to the original height.
        assertEquals(MAX_INIT_POS, mAttributeResults.get(length - 1).y);
    }

    @Test
    public void moveToTopThenMoveDown() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 500, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);
        strategy.setMockViewForTesting(mNavbar, mSpinnerView, mSpinner, mToolbarView,
                mToolbarCoordinator, mCoordinatorLayout);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertEquals(MAX_INIT_POS, mAttributeResults.get(0).y);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);

        // action down
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0));
        // action move
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1450, 0));
        // action move again
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1400, 0));
        // action up
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, DEVICE_WIDTH / 2, 1400, 0));

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();

        final int length = mAttributeResults.size();
        assertTrue(length > 1);
        // Move to cover the whole screen.
        assertEquals(0, mAttributeResults.get(length - 1).y);

        // action down
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 500, 0));
        // action move
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 650, 0));
        // action move again
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 700, 0));
        // action up
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, DEVICE_WIDTH / 2, 700, 0));

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();

        final int length2 = mAttributeResults.size();
        assertTrue(length2 > 1);

        // Back to the original height.
        assertEquals(MAX_INIT_POS, mAttributeResults.get(length2 - 1).y);
    }

    @Test
    public void moveDownToDismiss() {
        PartialCustomTabHeightStrategy strategy = new PartialCustomTabHeightStrategy(mActivity,
                mParentViewSupplier, 500, mMultiWindowModeStateDispatcher, null, null,
                mOnResizedCallback, mActivityLifecycleDispatcher);
        strategy.setMockViewForTesting(mNavbar, mSpinnerView, mSpinner, mToolbarView,
                mToolbarCoordinator, mCoordinatorLayout);

        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertEquals(MAX_INIT_POS, mAttributeResults.get(0).y);

        // Pass null because we have a mock Activity and we don't depend on the GestureDetector
        // inside as we test MotionEvents directly.
        PartialCustomTabHeightStrategy.PartialCustomTabHandleStrategy handleStrategy =
                strategy.new PartialCustomTabHandleStrategy(null);
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        // action down
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, DEVICE_WIDTH / 2, 1500, 0));
        // action move, the distance on y axis should be larger than
        // PartialCustomTabHandleStrategy.CLOSE_DISTANCE.
        handleStrategy.onTouchEvent(MotionEvent.obtain(SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(), MotionEvent.ACTION_MOVE, DEVICE_WIDTH / 2, 1850, 0));

        assertTrue("Close click handler should be called.", closed[0]);
    }

    private void verifyWindowFlagsSet() {
        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
    }
}
