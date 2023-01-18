// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.animation.Animator.AnimatorListener;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Insets;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSettings;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link PartialCustomTabHandleStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {PartialCustomTabHeightStrategyTest.ShadowSecureSettings.class})
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES,
        ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE,
        ChromeFeatureList.CCT_RESIZABLE_ALWAYS_SHOW_NAVBAR_BUTTONS})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabHeightStrategyTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    // Pixel 3 XL metrics
    private static final int DEVICE_HEIGHT = 2960;
    private static final int DEVICE_WIDTH = 1440;

    private static final int NAVBAR_HEIGHT = 160;
    private static final int INITIAL_HEIGHT = DEVICE_HEIGHT / 2 - NAVBAR_HEIGHT;
    private static final int FULL_HEIGHT = DEVICE_HEIGHT - NAVBAR_HEIGHT;
    private static final int MULTIWINDOW_HEIGHT = FULL_HEIGHT / 2;
    private static final int STATUS_BAR_HEIGHT = 68;

    private static final int FIND_TOOLBAR_COLOR = 3755;
    private static final int PCCT_TOOLBAR_COLOR = 12111;

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
    private ViewGroup mContentFrame;
    @Mock
    private ViewGroup mCoordinatorLayout;
    @Mock
    private View mDragBar;
    @Mock
    private View mDragHandlebar;
    @Mock
    private CommandLine mCommandLine;
    @Mock
    private FullscreenManager mFullscreenManager;
    @Mock
    private GradientDrawable mDragBarBackground;

    private Context mContext;
    private List<WindowManager.LayoutParams> mAttributeResults;
    private DisplayMetrics mRealMetrics;
    private DisplayMetrics mMetrics;
    private Callback<Integer> mBottomInsetCallback = inset -> {};
    private FrameLayout.LayoutParams mLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
    private FrameLayout.LayoutParams mCoordinatorLayoutParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
    private boolean mFullscreen;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        when(mActivity.getWindow()).thenReturn(mWindow);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getWindowManager()).thenReturn(mWindowManager);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view_stub)).thenReturn(mHandleViewStub);
        when(mActivity.findViewById(R.id.custom_tabs_handle_view)).thenReturn(mHandleView);
        when(mActivity.findViewById(R.id.drag_bar)).thenReturn(mDragBar);
        when(mActivity.findViewById(R.id.drag_handlebar)).thenReturn(mDragHandlebar);
        when(mActivity.findViewById(R.id.coordinator)).thenReturn(mCoordinatorLayout);
        when(mActivity.findViewById(android.R.id.content)).thenReturn(mContentFrame);
        when(mHandleView.getLayoutParams()).thenReturn(mLayoutParams);
        when(mToolbarCoordinator.getLayoutParams()).thenReturn(mLayoutParams);
        mAttributes = new WindowManager.LayoutParams();
        when(mWindow.getAttributes()).thenReturn(mAttributes);
        when(mWindow.getDecorView()).thenReturn(mDecorView);
        when(mWindow.getContext()).thenReturn(mActivity);
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
        when(mSpinnerView.getParent()).thenReturn(mContentFrame);
        when(mSpinnerView.animate()).thenReturn(mViewAnimator);
        when(mContentFrame.getLayoutParams()).thenReturn(mLayoutParams);
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mCoordinatorLayout.getLayoutParams()).thenReturn(mCoordinatorLayoutParams);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;

        mAttributeResults = new ArrayList<>();
        doAnswer(invocation -> {
            WindowManager.LayoutParams attributes = new WindowManager.LayoutParams();
            attributes.copyFrom((WindowManager.LayoutParams) invocation.getArgument(0));
            mAttributes.copyFrom(attributes);
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

        mContext = ApplicationProvider.getApplicationContext();
        ContextUtils.initApplicationContextForTests(mContext);
        CommandLine.setInstanceForTesting(mCommandLine);
    }

    @After
    public void tearDown() {
        // Reset the multi-window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
    }

    private PartialCustomTabHeightStrategy createPcctBackgroundDisabled() {
        PartialCustomTabHeightStrategy pcct =
                new PartialCustomTabHeightStrategy(mActivity, 500, false, mOnResizedCallback,
                        mActivityLifecycleDispatcher, mFullscreenManager, false, false);
        pcct.setMockViewForTesting(
                mNavbar, mSpinnerView, mSpinner, mToolbarView, mToolbarCoordinator);
        return pcct;
    }

    private PartialCustomTabHeightStrategy createPcctAtHeight(int heightPx) {
        return createPcctAtHeight(heightPx, false);
    }

    private PartialCustomTabHeightStrategy createPcctAtHeight(int heightPx, boolean isFixedHeight) {
        PartialCustomTabHeightStrategy pcct = new PartialCustomTabHeightStrategy(mActivity,
                heightPx, isFixedHeight, mOnResizedCallback, mActivityLifecycleDispatcher,
                mFullscreenManager, false, true);
        pcct.setMockViewForTesting(
                mNavbar, mSpinnerView, mSpinner, mToolbarView, mToolbarCoordinator);
        return pcct;
    }

    @Test
    public void create_heightIsCappedToHalfOfDeviceHeight() {
        createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));
    }

    @Test
    public void create_largeInitialHeight() {
        createPcctAtHeight(5000);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsFullHeight(mAttributeResults.get(0));
    }

    @Test
    public void create_heightIsCappedToDeviceHeight() {
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsFullHeight(mAttributeResults.get(0));
    }

    private void doTestHeightWithStatusBar() {
        when(mContentFrame.getHeight())
                .thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT);
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        verifyWindowFlagsSet();
        assertEquals(1, mAttributeResults.size());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_R() {
        configureStatusBarHeightForR();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mAttributeResults.get(0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_Q() {
        configureStatusBarHeightForQ();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mAttributeResults.get(0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_landscape_R() {
        configureStatusBarHeightForR();
        configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, mAttributeResults.get(0).height);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_landscape_Q() {
        configureStatusBarHeightForQ();
        configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, mAttributeResults.get(0).height);
    }

    @Test
    public void create_landscapeOrientation() {
        configLandscapeMode();
        createPcctAtHeight(800);
        verifyWindowFlagsSet();

        // Full height when in landscape mode.
        assertEquals(1, mAttributeResults.size());
        assertEquals(0, mAttributeResults.get(0).y);
    }

    @Test
    public void create_backgroundAppDisabledPortrait() {
        createPcctBackgroundDisabled();

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);

    }

    private static MotionEvent event(long ts, int action, int ypos) {
        return MotionEvent.obtain(ts, ts, action, DEVICE_WIDTH / 2, ypos, 0);
    }

    private static void actionDown(PartialCustomTabHandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_DOWN, ypos));
    }

    private static void actionMove(PartialCustomTabHandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_MOVE, ypos));
    }

    private static void actionUp(PartialCustomTabHandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_UP, ypos));
    }

    /**
     * Simulate dragging the tab and lifting the finger at the end.
     * @param handleStrategy {@link PartialCustomTabHandleStrategy} object.
     * @param ypos Series of y positions simulating the events.
     * @return Window attributes after the dragging finishes.
     */
    private WindowManager.LayoutParams dragTab(
            PartialCustomTabHandleStrategy handleStrategy, int... ypos) {
        int npos = ypos.length;
        assert npos >= 2;
        long timestamp = SystemClock.uptimeMillis();

        // ACTION_DOWN -> ACTION_MOVE * (npos-1) -> ACTION_UP
        actionDown(handleStrategy, timestamp, ypos[0]);
        for (int i = 1; i < npos; ++i) actionMove(handleStrategy, timestamp, ypos[i]);
        actionUp(handleStrategy, timestamp, ypos[npos - 1]);

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        int length = mAttributeResults.size();
        assertTrue(length > 1);
        return mAttributeResults.get(length - 1);
    }

    private void assertMotionEventIgnored(PartialCustomTabHandleStrategy handleStrategy) {
        assertFalse(handleStrategy.onInterceptTouchEvent(
                event(SystemClock.uptimeMillis(), MotionEvent.ACTION_DOWN, 1500)));
    }

    private void assertTabIsAtInitialPos(WindowManager.LayoutParams attrs) {
        assertEquals(INITIAL_HEIGHT + NAVBAR_HEIGHT, attrs.y);
    }

    private void assertTabIsFullHeight(WindowManager.LayoutParams attrs) {
        assertEquals(FULL_HEIGHT, attrs.height);
    }

    private void assertTabBelowStatusBar(WindowManager.LayoutParams attrs) {
        assertEquals(FULL_HEIGHT - STATUS_BAR_HEIGHT, attrs.height);
    }

    private void disableSpinnerAnimation() {
        // Disable animation for the mock spinner view.
        doAnswer(invocation -> {
            AnimatorListener listener = invocation.getArgument(0);
            listener.onAnimationEnd(null);
            return mViewAnimator;
        })
                .when(mViewAnimator)
                .setListener(any(AnimatorListener.class));
    }

    private WindowManager.LayoutParams getWindowAttributes() {
        return mAttributeResults.get(mAttributeResults.size() - 1);
    }

    private void configPortraitMode() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        mRealMetrics.widthPixels = DEVICE_WIDTH;
        mRealMetrics.heightPixels = DEVICE_HEIGHT;
        when(mContentFrame.getHeight()).thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT);
        when(mDisplay.getRotation()).thenReturn(Surface.ROTATION_90);
    }

    private void configLandscapeMode() {
        configLandscapeMode(Surface.ROTATION_90);
    }

    private void configLandscapeMode(int direction) {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mRealMetrics.widthPixels = DEVICE_HEIGHT;
        mRealMetrics.heightPixels = DEVICE_WIDTH;
        when(mContentFrame.getHeight()).thenReturn(DEVICE_WIDTH);
        when(mDisplay.getRotation()).thenReturn(direction);
    }

    private void configureStatusBarHeightForR() {
        // Setup for R+
        WindowMetrics windowMetric = Mockito.mock(WindowMetrics.class);
        WindowInsets windowInsets = Mockito.mock(WindowInsets.class);

        doReturn(windowMetric).when(mWindowManager).getCurrentWindowMetrics();
        doReturn(windowInsets).when(windowMetric).getWindowInsets();
        doReturn(new Rect(0, 0, mRealMetrics.widthPixels, mRealMetrics.heightPixels))
                .when(windowMetric)
                .getBounds();
        doReturn(Insets.of(0, STATUS_BAR_HEIGHT, 0, 0))
                .when(windowInsets)
                .getInsets(eq(WindowInsets.Type.statusBars()));
        doReturn(Insets.of(0, 0, 0, NAVBAR_HEIGHT))
                .when(windowInsets)
                .getInsets(eq(WindowInsets.Type.navigationBars()));
    }

    private void configureStatusBarHeightForQ() {
        // Setup for Q-
        int statusBarId = 54321;
        doReturn(statusBarId).when(mResources).getIdentifier(eq("status_bar_height"), any(), any());
        doReturn(STATUS_BAR_HEIGHT).when(mResources).getDimensionPixelSize(eq(statusBarId));
    }

    private void verifyWindowFlagsSet() {
        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
    }

    @Test
    public void moveFromTop() {
        // Drag to the top
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));

        // Drag down a little -> slide back to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 50, 100, 150));

        // Drag down enough -> slide to the initial position.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));
    }

    @Test
    public void moveFromInitialHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());

        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag up slightly -> slide back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1400));

        // Drag down slightly -> slide back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void moveUpThenDown() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Shake the tab from the initial position slightly -> back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1600));
    }

    @Test
    public void moveUp_landscapeOrientationUnresizable() {
        configLandscapeMode();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveUp_multiwindowModeUnresizable() {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void rotateToLandscapeUnresizable() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void rotateToLandscapeAndBackTestHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged((mConfiguration));
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mConfiguration);
        assertTabIsFullHeight(mAttributeResults.get(mAttributeResults.size()-1));
    }

    @Test
    public void showDragHandleOnPortraitMode() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        verify(mDragBar).setVisibility(View.VISIBLE);
        clearInvocations(mDragBar);

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);
        verify(mDragBar).setVisibility(View.GONE);
        clearInvocations(mDragBar);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mConfiguration);
        verify(mDragBar).setVisibility(View.VISIBLE);
        clearInvocations(mDragBar);

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        strategy.onConfigurationChanged(mConfiguration);
        verify(mDragBar).setVisibility(View.GONE);
    }

    @Test
    public void enterMultiwindowModeUnresizable() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        strategy.onConfigurationChanged(mConfiguration);
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveDownToDismiss() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("Close click handler should be called.", closed[0]);
    }

    @Test
    public void showSpinnerOnDragUpOnly() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        actionUp(handleStrategy, timestamp, 1450);

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();
        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        // Now the tab is full-height. Start dragging down.
        actionDown(handleStrategy, timestamp, 500);
        actionMove(handleStrategy, timestamp, 650);

        // Verify the spinner remained invisible.
        verify(mSpinnerView, never()).setVisibility(anyInt());
    }

    @Test
    public void hideSpinnerWhenReachingFullHeight() {
        disableSpinnerAnimation();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        // Verify the spinner remains invisible after the tab reaches the top.
        int topY = strategy.getFullyExpandedYWithAdjustment();
        actionMove(handleStrategy, timestamp, topY);
        verify(mSpinnerView).setVisibility(View.GONE);
        clearInvocations(mSpinnerView);

        actionMove(handleStrategy, timestamp, topY + 200);
        verify(mSpinnerView, never()).setVisibility(anyInt());
    }

    @Test
    public void hideSpinnerWhenDraggingDown() {
        disableSpinnerAnimation();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);

        verify(mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        // Drag below the initial height.
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT + 100);

        // Verify the spinner goes invisible.
        verify(mSpinnerView).setVisibility(View.GONE);
    }

    @Test
    public void hideSpinnerEarly() {
        // Test hiding spinner early (500ms after showing) when there is no glitch at
        // the end of draggin action.
        disableSpinnerAnimation();
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        when(mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        verify(mSpinnerView).setVisibility(View.VISIBLE);
        when(mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mSpinnerView);

        long timeOut = PartialCustomTabHeightStrategy.SPINNER_TIMEOUT_MS;
        shadowOf(Looper.getMainLooper()).idleFor(timeOut, TimeUnit.MILLISECONDS);

        // Verify the spinner goes invisible after the specified timeout.
        verify(mSpinnerView).setVisibility(View.GONE);
    }

    @Test
    public void expandToFullHeightOnShowingKeyboard() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));
        int expected = PartialCustomTabHeightStrategy.ResizeType.AUTO_EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType2", expected);

        strategy.onShowSoftInput(() -> {});
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        final int length = mAttributeResults.size();
        assertTrue(length > 1);

        // Verify that the tab expands to full height.
        assertTabIsFullHeight(mAttributeResults.get(length - 1));
        assertEquals("ResizeType.AUTO_EXPANSION should be recorded once.", 1,
                histogramExpansion.getDelta());
        waitForAnimationToFinish();
        verify(mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
    }

    @Test
    public void moveUpFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();

        // Try to drag up and verify that the location does not change.
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 250);
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void moveDownFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Try to drag down and check that it returns to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void moveDownToDismissFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("Close click handler should be called.", closed[0]);
    }

    @Test
    public void dragHandlebarInvisibleFixedHeight() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500, true);
        verifyWindowFlagsSet();

        assertEquals(1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        verify(mDragHandlebar).setVisibility(View.GONE);
    }

    @Test
    public void invokeResizeCallbackExpansion() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(
                "mAttributeResults should have exactly 1 element.", 1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        int expected = PartialCustomTabHeightStrategy.ResizeType.MANUAL_EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType2", expected);

        // Drag to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        // invokeResizeCallback() should have been called and MANUAL_EXPANSION logged once.
        assertEquals("ResizeType.MANUAL_EXPANSION should be recorded once.", 1,
                histogramExpansion.getDelta());
    }

    @Test
    public void invokeResizeCallbackMinimization() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(
                "mAttributeResults should have exactly 1 element.", 1, mAttributeResults.size());
        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top so it can be minimized in the next step.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        int expected = PartialCustomTabHeightStrategy.ResizeType.MANUAL_MINIMIZATION;
        HistogramDelta histogramMinimization =
                new HistogramDelta("CustomTabs.ResizeType2", expected);

        // Drag down enough -> slide to the initial position.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));

        // invokeResizeCallback() should have been called and MANUAL_MINIMIZATION logged once.
        assertEquals("ResizeType.MANUAL_MINIMIZATION should be recorded once.", 1,
                histogramMinimization.getDelta());
    }

    @Test
    public void callbackWhenHeightResized() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertTabIsAtInitialPos(mAttributeResults.get(0));
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Slide back to the initial height -> no resize happens.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1400));
        verify(mOnResizedCallback, never()).onResized(anyInt(), anyInt());

        // Drag to the top -> resized.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));
        verify(mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
        clearInvocations(mOnResizedCallback);

        // Slide back to the top -> no resize happens.
        assertTabIsFullHeight(dragTab(handleStrategy, 50, 100, 150));
        verify(mOnResizedCallback, never()).onResized(anyInt(), anyInt());

        // Drag to the initial height -> resized.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));
        verify(mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
    }

    @Test
    public void callbackUponRotation() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);

        configLandscapeMode();
        strategy.onConfigurationChanged(mConfiguration);
        verify(mOnResizedCallback).onResized(eq(DEVICE_WIDTH), eq(DEVICE_HEIGHT));
        clearInvocations(mOnResizedCallback);

        configPortraitMode();
        strategy.onConfigurationChanged(mConfiguration);
        verify(mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
    }

    @Test
    public void verifyNavigationBarHeightInMultiWindowMode() {
        mMetrics = new DisplayMetrics();
        mMetrics.widthPixels = DEVICE_WIDTH;
        mMetrics.heightPixels = MULTIWINDOW_HEIGHT;
        doAnswer(invocation -> {
            DisplayMetrics displayMetrics = invocation.getArgument(0);
            displayMetrics.setTo(mMetrics);
            return null;
        })
                .when(mDisplay)
                .getMetrics(any(DisplayMetrics.class));
        when(mContentFrame.getHeight()).thenReturn(MULTIWINDOW_HEIGHT);
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertEquals(0, strategy.getNavbarHeightForTesting());
    }

    @Test
    public void adjustWidthInLandscapeMode() {
        configLandscapeMode(Surface.ROTATION_90);
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        WindowManager.LayoutParams attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);

        configPortraitMode();
        strategy.onConfigurationChanged(mConfiguration);
        attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);

        configLandscapeMode(Surface.ROTATION_270);
        strategy.onConfigurationChanged(mConfiguration);
        attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);
    }

    @Test
    public void enterAndExitHtmlFullscreen() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        assertTrue(getWindowAttributes().isFullscreen());
        verify(mOnResizedCallback).onResized(eq(DEVICE_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mOnResizedCallback);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        waitForAnimationToFinish();
        assertFalse(getWindowAttributes().isFullscreen());
        assertEquals(height, getWindowAttributes().height);
        verify(mOnResizedCallback).onResized(eq(height), anyInt());
    }

    @Test
    public void fullscreenInLandscapeMode() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        int height = getWindowAttributes().height;

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        mFullscreen = false;
        strategy.onExitFullscreen(null);
        waitForAnimationToFinish();

        assertEquals(0, getWindowAttributes().y);
    }

    @Test
    public void rotateAcrossFullscreenMode() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        int height = getWindowAttributes().height;

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mConfiguration);

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);

        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mConfiguration);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        waitForAnimationToFinish();

        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void dragToTheSameInitialY() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        verifyWindowFlagsSet();

        assertEquals(
                "mAttributeResults should have exactly 1 element.", 1, mAttributeResults.size());

        assertTabIsAtInitialPos(mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag tab slightly but actionDown and actionUp will be performed at the same Y.
        // The tab should remain open.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1500));
    }

    @Test
    public void dragBarMatchesFindToolbarInColor() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(500);
        strategy.setToolbarColorForTesting(PCCT_TOOLBAR_COLOR);
        doReturn(FIND_TOOLBAR_COLOR)
                .when(mResources)
                .getColor(eq(R.color.find_in_page_background_color));
        doReturn(mDragBarBackground).when(mDragBar).getBackground();

        strategy.onFindToolbarShown();
        verify(mDragBarBackground).setColor(FIND_TOOLBAR_COLOR);

        strategy.onFindToolbarHidden();
        verify(mDragBarBackground).setColor(PCCT_TOOLBAR_COLOR);
    }

    @Implements(Settings.Secure.class)
    static class ShadowSecureSettings extends ShadowSettings.ShadowSecure {
        public static String sImmersiveModeConfirmationsValue;

        @Implementation
        protected static String getString(ContentResolver resolver, String name) {
            if (name.equals(PartialCustomTabHeightStrategy.IMMERSIVE_MODE_CONFIRMATIONS_SETTING)) {
                return sImmersiveModeConfirmationsValue;
            }
            return null;
        }
    }

    @Test
    public void logImmersiveModeConfirmationsSettingConfirmed() {
        PartialCustomTabHeightStrategy.setHasLoggedImmersiveModeConfirmationSettingForTesting(
                false);
        ShadowSecureSettings.sImmersiveModeConfirmationsValue =
                PartialCustomTabHeightStrategy.IMMERSIVE_MODE_CONFIRMATIONS_SETTING_VALUE;

        int expectedLoggedValue = 1;
        HistogramDelta histogramDelta = new HistogramDelta(
                "CustomTabs.ImmersiveModeConfirmationsSettingConfirmed", expectedLoggedValue);

        createPcctAtHeight(500);

        assertEquals(
                "CustomTabs.ImmersiveModeConfirmationsSettingConfirmed should be recorded once.", 1,
                histogramDelta.getDelta());
    }

    @Test
    public void logImmersiveModeConfirmationsSettingEmpty() {
        PartialCustomTabHeightStrategy.setHasLoggedImmersiveModeConfirmationSettingForTesting(
                false);
        ShadowSecureSettings.sImmersiveModeConfirmationsValue = "";

        int expectedLoggedValue = 0;
        HistogramDelta histogramDelta = new HistogramDelta(
                "CustomTabs.ImmersiveModeConfirmationsSettingConfirmed", expectedLoggedValue);

        createPcctAtHeight(500);

        assertEquals(
                "CustomTabs.ImmersiveModeConfirmationsSettingConfirmed should be recorded once.", 1,
                histogramDelta.getDelta());
    }

    @Test
    public void logImmersiveModeConfirmationsSettingConfirmedLogOnlyOnce() {
        PartialCustomTabHeightStrategy.setHasLoggedImmersiveModeConfirmationSettingForTesting(
                false);
        ShadowSecureSettings.sImmersiveModeConfirmationsValue =
                PartialCustomTabHeightStrategy.IMMERSIVE_MODE_CONFIRMATIONS_SETTING_VALUE;

        int expectedLoggedValue = 1;
        HistogramDelta histogramDelta = new HistogramDelta(
                "CustomTabs.ImmersiveModeConfirmationsSettingConfirmed", expectedLoggedValue);

        createPcctAtHeight(500);
        createPcctAtHeight(500);

        assertEquals(
                "CustomTabs.ImmersiveModeConfirmationsSettingConfirmed should be recorded once.", 1,
                histogramDelta.getDelta());
    }

    @Test
    public void noTopShadowAtFullHeight() {
        doReturn(47).when(mResources).getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertNotEquals("Top margin should be non-zero for the shadow", 0, mLayoutParams.topMargin);

        dragTab(handleStrategy, 1500, 1000, 500);
        assertEquals("There should be no top shadow at full height", 0, mLayoutParams.topMargin);
    }

    @Test
    public void expandToFullHeightOnFindInPage() {
        PartialCustomTabHeightStrategy strategy = createPcctAtHeight(800);
        doReturn(mDragBarBackground).when(mDragBar).getBackground();
        int expected = PartialCustomTabHeightStrategy.ResizeType.AUTO_EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType2", expected);
        strategy.onFindToolbarShown();
        waitForAnimationToFinish();

        assertTabIsFullHeight(getWindowAttributes());
        assertEquals("ResizeType.AUTO_EXPANSION should be recorded once.", 1,
                histogramExpansion.getDelta());
        verify(mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
        clearInvocations(mOnResizedCallback);

        expected = PartialCustomTabHeightStrategy.ResizeType.AUTO_MINIMIZATION;
        HistogramDelta histogramMinimization =
                new HistogramDelta("CustomTabs.ResizeType2", expected);
        strategy.onFindToolbarHidden();
        waitForAnimationToFinish();

        assertTabIsAtInitialPos(getWindowAttributes());
        assertEquals("ResizeType.AUTO_MINIMIZATION should be recorded once.", 1,
                histogramMinimization.getDelta());
        verify(mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
    }

    private static void waitForAnimationToFinish() {
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }
}
