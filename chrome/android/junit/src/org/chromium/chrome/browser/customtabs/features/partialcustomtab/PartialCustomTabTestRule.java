// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Surface;
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
import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TouchEventProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * A TestRule that sets up the mocks and contains helper methods for JUnit/Robolectric tests scoped
 * to the Partial Custom Tabs logic.
 */
public class PartialCustomTabTestRule implements TestRule {
    @Implements(SemanticColorUtils.class)
    static class ShadowSemanticColorUtils {
        @Implementation
        public static int getDividerLineBgColor(Context context) {
            return Color.LTGRAY;
        }
    }

    // Pixel 3 XL metrics
    static final float DENSITY = 1.25f;
    static final int DEVICE_HEIGHT = 2960;
    static final int DEVICE_WIDTH = 1440;
    static final int DEVICE_HEIGHT_LANDSCAPE = DEVICE_WIDTH;
    static final int DEVICE_WIDTH_LANDSCAPE = DEVICE_HEIGHT;
    static final int NAVBAR_HEIGHT = 160;
    static final int STATUS_BAR_HEIGHT = 68;
    static final int FULL_HEIGHT = DEVICE_HEIGHT - NAVBAR_HEIGHT;
    static final int MULTIWINDOW_HEIGHT = FULL_HEIGHT / 2;

    static final int DEVICE_WIDTH_MEDIUM = 700;
    static final int DEVICE_WIDTH_COMPACT = 500;
    static final int DEVICE_HEIGHT_COMPACT = 300;
    static final int DEVICE_WIDTH_COMPACT_PORTRAIT = DEVICE_HEIGHT_COMPACT;
    static final int DEVICE_HEIGHT_COMPACT_PORTRAIT = DEVICE_WIDTH_COMPACT;

    @Mock Activity mActivity;
    @Mock Window mWindow;
    @Mock WindowManager mWindowManager;
    @Mock Resources mResources;
    @Mock Configuration mConfiguration;
    WindowManager.LayoutParams mAttributes;
    @Mock TouchEventProvider mTouchEventProvider;
    @Mock Tab mTab;
    @Mock View mDecorView;
    @Mock View mRootView;
    @Mock Display mDisplay;
    @Mock BrowserServicesIntentDataProvider mIntentData;
    @Mock CustomTabHeightStrategy.OnResizedCallback mOnResizedCallback;
    @Mock CustomTabHeightStrategy.OnActivityLayoutCallback mOnActivityLayoutCallback;
    @Mock ViewGroup mCoordinatorLayout;
    @Mock ViewGroup mContentFrame;
    @Mock FullscreenManager mFullscreenManager;
    @Mock ViewStub mHandleViewStub;
    @Mock ImageView mHandleView;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock LinearLayout mNavbar;
    @Mock ViewPropertyAnimator mViewAnimator;
    @Mock ImageView mSpinnerView;
    @Mock CircularProgressDrawable mSpinner;
    @Mock CustomTabToolbar mToolbarView;
    @Mock View mToolbarCoordinator;
    @Mock CustomTabDragBar mDragBar;
    @Mock View mDragHandlebar;
    @Mock GradientDrawable mDragBarBackground;
    @Mock InsetDrawable mInsetDragBarBackground;
    @Mock ColorDrawable mColorDrawable;
    @Mock PartialCustomTabHandleStrategyFactory mHandleStrategyFactory;
    @Mock DisplayMetrics mMetrics;
    @Mock ViewGroup mCompositorViewHolder;
    @Mock PackageManager mPackageManager;
    @Mock ActivityManager mActivityManager;
    @Captor ArgumentCaptor<View.OnAttachStateChangeListener> mAttachStateChangeListener;

    Context mContext;
    List<WindowManager.LayoutParams> mAttributeResults;
    DisplayMetrics mRealMetrics;
    Point mDisplaySize;

    FrameLayout.LayoutParams mLayoutParams =
            new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
    FrameLayout.LayoutParams mCoordinatorLayoutParams =
            new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
    ViewGroup.LayoutParams mDragBarLayoutParams =
            new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);

    private void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getWindowManager()).thenReturn(mWindowManager);
        when(mActivity.findViewById(R.id.coordinator)).thenReturn(mCoordinatorLayout);
        when(mActivity.findViewById(R.id.compositor_view_holder)).thenReturn(mCompositorViewHolder);
        when(mActivity.findViewById(android.R.id.content)).thenReturn(mContentFrame);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view_stub)).thenReturn(mHandleViewStub);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view)).thenReturn(mHandleView);
        when(mActivity.findViewById(R.id.drag_bar)).thenReturn(mDragBar);
        when(mActivity.findViewById(R.id.drag_handle)).thenReturn(mDragHandlebar);
        mAttributes = new WindowManager.LayoutParams();
        when(mWindow.getAttributes()).thenReturn(mAttributes);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mWindow.getContext()).thenReturn(mActivity);
        when(mDecorView.getRootView()).thenReturn(mRootView);
        when(mRootView.getLayoutParams()).thenReturn(mAttributes);
        when(mWindowManager.getDefaultDisplay()).thenReturn(mDisplay);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        mMetrics.density = DENSITY;
        when(mResources.getDisplayMetrics()).thenReturn(mMetrics);
        when(mContentFrame.getLayoutParams()).thenReturn(mLayoutParams);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mCoordinatorLayout.getLayoutParams()).thenReturn(mCoordinatorLayoutParams);
        when(mCoordinatorLayout.getBackground()).thenReturn(mDragBarBackground);
        when(mHandleView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mHandleView.getBackground()).thenReturn(mDragBarBackground);
        when(mHandleView.findViewById(R.id.drag_bar)).thenReturn(mDragBar);
        when(mToolbarCoordinator.getLayoutParams()).thenReturn(mLayoutParams);
        when(mNavbar.animate()).thenReturn(mViewAnimator);
        when(mViewAnimator.alpha(anyFloat())).thenReturn(mViewAnimator);
        when(mViewAnimator.setDuration(anyLong())).thenReturn(mViewAnimator);
        when(mViewAnimator.setListener(any())).thenReturn(mViewAnimator);
        when(mSpinnerView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mSpinnerView.getParent()).thenReturn(mContentFrame);
        when(mSpinnerView.animate()).thenReturn(mViewAnimator);
        when(mToolbarView.getBackground()).thenReturn(mColorDrawable);
        when(mToolbarView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mColorDrawable.getColor()).thenReturn(2);
        when(mDragBar.getBackground()).thenReturn(mDragBarBackground);
        when(mDragBar.getLayoutParams()).thenReturn(mDragBarLayoutParams);
        when(mHandleStrategyFactory.create(
                        anyInt(),
                        any(Context.class),
                        any(BooleanSupplier.class),
                        any(Supplier.class),
                        any(PartialCustomTabHandleStrategy.DragEventCallback.class)))
                .thenReturn(null);
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;

        mAttributeResults = new ArrayList<>();
        doAnswer(
                        invocation -> {
                            WindowManager.LayoutParams attributes =
                                    new WindowManager.LayoutParams();
                            attributes.copyFrom(
                                    (WindowManager.LayoutParams) invocation.getArgument(0));
                            mAttributes.copyFrom(attributes);
                            mAttributeResults.add(attributes);
                            return null;
                        })
                .when(mWindow)
                .setAttributes(any(WindowManager.LayoutParams.class));

        mRealMetrics = new DisplayMetrics();
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        mRealMetrics.density = DENSITY;
        doAnswer(
                        invocation -> {
                            DisplayMetrics displayMetrics = invocation.getArgument(0);
                            displayMetrics.setTo(mRealMetrics);
                            return null;
                        })
                .when(mDisplay)
                .getRealMetrics(any(DisplayMetrics.class));

        mDisplaySize = new Point(DEVICE_WIDTH, DEVICE_HEIGHT - NAVBAR_HEIGHT);
        doAnswer(
                        invocation -> {
                            Point size = invocation.getArgument(0);
                            size.set(mDisplaySize.x, mDisplaySize.y);
                            return null;
                        })
                .when(mDisplay)
                .getSize(any(Point.class));
        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        when(mActivity.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
        when(mActivity.getPackageManager()).thenReturn(mPackageManager);
    }

    private void commonTearDown() {
        // Reset the multi-window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
    }

    public static void waitForAnimationToFinish() {
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    public void configPortraitMode() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        mRealMetrics.density = DENSITY;
        mDisplaySize.set(DEVICE_WIDTH, DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mDisplay.getRotation()).thenReturn(Surface.ROTATION_90);
    }

    public void configInsetDrawableBg() {
        when(mDragBar.getBackground()).thenReturn(mInsetDragBarBackground);
        when(mInsetDragBarBackground.getDrawable()).thenReturn(mDragBarBackground);
    }

    public void configLandscapeMode() {
        configLandscapeMode(Surface.ROTATION_90);
    }

    public void configLandscapeMode(int direction) {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        mRealMetrics.density = DENSITY;
        mDisplaySize.set(DEVICE_HEIGHT, DEVICE_WIDTH);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_WIDTH);
        when(mDisplay.getRotation()).thenReturn(direction);
    }

    public void configDeviceWidthMedium() {
        mRealMetrics.widthPixels = DEVICE_WIDTH_MEDIUM;
        mDisplaySize.set(DEVICE_WIDTH_MEDIUM, DEVICE_HEIGHT - NAVBAR_HEIGHT);
    }

    public void configCompactDevice() {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_WIDTH_COMPACT;
        mRealMetrics.heightPixels = DEVICE_HEIGHT_COMPACT;
        mDisplaySize.set(DEVICE_WIDTH_COMPACT, DEVICE_HEIGHT_COMPACT);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_WIDTH_COMPACT);
    }

    public void configCompactDevice_Portrait() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mRealMetrics.widthPixels = DEVICE_WIDTH_COMPACT_PORTRAIT;
        mRealMetrics.heightPixels = DEVICE_HEIGHT_COMPACT_PORTRAIT;
        mDisplaySize.set(DEVICE_WIDTH_COMPACT_PORTRAIT, DEVICE_HEIGHT_COMPACT_PORTRAIT);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT_COMPACT_PORTRAIT);
    }

    public void verifyWindowFlagsSet() {
        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
    }

    public WindowManager.LayoutParams getWindowAttributes() {
        return mAttributeResults.get(mAttributeResults.size() - 1);
    }

    public float getDisplayDensity() {
        return mActivity.getResources().getDisplayMetrics().density;
    }

    public void setupDisplayMetricsInMultiWindowMode() {
        mMetrics = new DisplayMetrics();
        mMetrics.widthPixels = DEVICE_WIDTH;
        mMetrics.heightPixels = MULTIWINDOW_HEIGHT;
        doAnswer(
                        invocation -> {
                            DisplayMetrics displayMetrics = invocation.getArgument(0);
                            displayMetrics.setTo(mMetrics);
                            return null;
                        })
                .when(mDisplay)
                .getMetrics(any(DisplayMetrics.class));
        mDisplaySize.y = MULTIWINDOW_HEIGHT;
        when(mContentFrame.getHeight()).thenReturn(MULTIWINDOW_HEIGHT);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                try {
                    statement.evaluate();
                } finally {
                    commonTearDown();
                }
            }
        };
    }
}
