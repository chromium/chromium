// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.FULL_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.NAVBAR_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.STATUS_BAR_HEIGHT;

import android.animation.Animator.AnimatorListener;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.util.browser.Features;

import java.util.concurrent.TimeUnit;

/** Tests for {@link PartialCustomTabHandleStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES,
        ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabBottomSheetStrategyTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private static final int INITIAL_HEIGHT = DEVICE_HEIGHT / 2 - NAVBAR_HEIGHT;
    private static final int MULTIWINDOW_HEIGHT = FULL_HEIGHT / 2;

    private static final int FIND_TOOLBAR_COLOR = 3755;
    private static final int PCCT_TOOLBAR_COLOR = 12111;

    private boolean mFullscreen;

    private PartialCustomTabBottomSheetStrategy createPcctBackgroundDisabled() {
        PartialCustomTabBottomSheetStrategy pcct = new PartialCustomTabBottomSheetStrategy(
                mPCCTTestRule.mActivity, 500, false, mPCCTTestRule.mOnResizedCallback,
                mPCCTTestRule.mActivityLifecycleDispatcher, mPCCTTestRule.mFullscreenManager, false,
                false, /*startMaximized=*/false, mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(mPCCTTestRule.mNavbar, mPCCTTestRule.mSpinnerView,
                mPCCTTestRule.mSpinner, mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mHandleStrategyFactory);
        return pcct;
    }

    private PartialCustomTabBottomSheetStrategy createPcctAtHeight(int heightPx) {
        return createPcctAtHeight(heightPx, false);
    }

    private PartialCustomTabBottomSheetStrategy createPcctAtHeight(
            int heightPx, boolean isFixedHeight) {
        PartialCustomTabBottomSheetStrategy pcct = new PartialCustomTabBottomSheetStrategy(
                mPCCTTestRule.mActivity, heightPx, isFixedHeight, mPCCTTestRule.mOnResizedCallback,
                mPCCTTestRule.mActivityLifecycleDispatcher, mPCCTTestRule.mFullscreenManager, false,
                true, /*startMaxmized=*/false, mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(mPCCTTestRule.mNavbar, mPCCTTestRule.mSpinnerView,
                mPCCTTestRule.mSpinner, mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mHandleStrategyFactory);
        return pcct;
    }

    @Test
    public void create_heightIsCappedToHalfOfDeviceHeight() {
        createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));
    }

    @Test
    public void create_largeInitialHeight() {
        createPcctAtHeight(5000);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsFullHeight(mPCCTTestRule.mAttributeResults.get(0));
    }

    @Test
    public void create_heightIsCappedToDeviceHeight() {
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsFullHeight(mPCCTTestRule.mAttributeResults.get(0));
    }

    private void doTestHeightWithStatusBar() {
        when(mPCCTTestRule.mContentFrame.getHeight())
                .thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT);
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        mPCCTTestRule.verifyWindowFlagsSet();
        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_R() {
        configureStatusBarHeightForR();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mPCCTTestRule.mAttributeResults.get(0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_Q() {
        configureStatusBarHeightForQ();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mPCCTTestRule.mAttributeResults.get(0));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_landscape_R() {
        configureStatusBarHeightForR();
        mPCCTTestRule.configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_landscape_Q() {
        configureStatusBarHeightForQ();
        mPCCTTestRule.configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
    }

    @Test
    public void create_landscapeOrientation() {
        mPCCTTestRule.configLandscapeMode();
        createPcctAtHeight(800);
        mPCCTTestRule.verifyWindowFlagsSet();

        // Full height when in landscape mode.
        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertEquals(0, mPCCTTestRule.mAttributeResults.get(0).y);
    }

    @Test
    public void create_backgroundAppDisabledPortrait() {
        createPcctBackgroundDisabled();

        verify(mPCCTTestRule.mWindow).addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        verify(mPCCTTestRule.mWindow).clearFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
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
        int length = mPCCTTestRule.mAttributeResults.size();
        assertTrue(length > 1);
        return mPCCTTestRule.mAttributeResults.get(length - 1);
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
            return mPCCTTestRule.mViewAnimator;
        })
                .when(mPCCTTestRule.mViewAnimator)
                .setListener(any(AnimatorListener.class));
    }

    private WindowManager.LayoutParams getWindowAttributes() {
        return mPCCTTestRule.mAttributeResults.get(mPCCTTestRule.mAttributeResults.size() - 1);
    }

    private void configureStatusBarHeightForR() {
        // Setup for R+
        WindowMetrics windowMetric = Mockito.mock(WindowMetrics.class);
        WindowInsets windowInsets = Mockito.mock(WindowInsets.class);

        doReturn(windowMetric).when(mPCCTTestRule.mWindowManager).getCurrentWindowMetrics();
        doReturn(windowInsets).when(windowMetric).getWindowInsets();
        doReturn(new Rect(0, 0, mPCCTTestRule.mRealMetrics.widthPixels,
                         mPCCTTestRule.mRealMetrics.heightPixels))
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
        doReturn(statusBarId)
                .when(mPCCTTestRule.mResources)
                .getIdentifier(eq("status_bar_height"), any(), any());
        doReturn(STATUS_BAR_HEIGHT)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(statusBarId));
    }

    @Test
    public void moveFromTop() {
        // Drag to the top
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

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
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());

        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag up slightly -> slide back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1400));

        // Drag down slightly -> slide back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void moveUpThenDown() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        verify(mPCCTTestRule.mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mPCCTTestRule.mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Shake the tab from the initial position slightly -> back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1600));
    }

    @Test
    public void moveUp_landscapeOrientationUnresizable() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveUp_multiwindowModeUnresizable() {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void rotateToLandscapeUnresizable() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void rotateToLandscapeAndBackTestHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged((mPCCTTestRule.mConfiguration));
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertTabIsFullHeight(
                mPCCTTestRule.mAttributeResults.get(mPCCTTestRule.mAttributeResults.size() - 1));
    }

    @Test
    public void showDragHandleOnPortraitMode() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        verify(mPCCTTestRule.mDragBar).setVisibility(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mDragBar);

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        verify(mPCCTTestRule.mDragBar).setVisibility(View.GONE);
        clearInvocations(mPCCTTestRule.mDragBar);

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        verify(mPCCTTestRule.mDragBar).setVisibility(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mDragBar);

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        verify(mPCCTTestRule.mDragBar).setVisibility(View.GONE);
    }

    @Test
    public void enterMultiwindowModeUnresizable() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveDownToDismiss() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("Close click handler should be called.", closed[0]);
    }

    @Test
    public void showSpinnerOnDragUpOnly() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        verify(mPCCTTestRule.mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mPCCTTestRule.mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.VISIBLE);
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mSpinnerView);

        actionUp(handleStrategy, timestamp, 1450);

        // Wait animation to finish.
        shadowOf(Looper.getMainLooper()).idle();
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        // Now the tab is full-height. Start dragging down.
        actionDown(handleStrategy, timestamp, 500);
        actionMove(handleStrategy, timestamp, 650);

        // Verify the spinner remained invisible.
        verify(mPCCTTestRule.mSpinnerView, never()).setVisibility(anyInt());
    }

    @Test
    public void hideSpinnerWhenReachingFullHeight() {
        disableSpinnerAnimation();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        verify(mPCCTTestRule.mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mPCCTTestRule.mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.VISIBLE);
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mSpinnerView);

        // Verify the spinner remains invisible after the tab reaches the top.
        int topY = strategy.getFullyExpandedYWithAdjustment();
        actionMove(handleStrategy, timestamp, topY);
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.GONE);
        clearInvocations(mPCCTTestRule.mSpinnerView);

        actionMove(handleStrategy, timestamp, topY + 200);
        verify(mPCCTTestRule.mSpinnerView, never()).setVisibility(anyInt());
    }

    @Test
    public void hideSpinnerWhenDraggingDown() {
        disableSpinnerAnimation();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        verify(mPCCTTestRule.mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mPCCTTestRule.mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.VISIBLE);
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mSpinnerView);

        // Drag below the initial height.
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT + 100);

        // Verify the spinner goes invisible.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.GONE);
    }

    @Test
    public void hideSpinnerEarly() {
        // Test hiding spinner early (500ms after showing) when there is no glitch at
        // the end of draggin action.
        disableSpinnerAnimation();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.VISIBLE);
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mSpinnerView);

        long timeOut = PartialCustomTabBottomSheetStrategy.SPINNER_TIMEOUT_MS;
        shadowOf(Looper.getMainLooper()).idleFor(timeOut, TimeUnit.MILLISECONDS);

        // Verify the spinner goes invisible after the specified timeout.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.GONE);
    }

    @Test
    public void expandToFullHeightOnShowingKeyboard() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));
        int expected = PartialCustomTabBottomSheetStrategy.ResizeType.AUTO_EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType2", expected);

        strategy.onShowSoftInput(() -> {});
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        final int length = mPCCTTestRule.mAttributeResults.size();
        assertTrue(length > 1);

        // Verify that the tab expands to full height.
        assertTabIsFullHeight(mPCCTTestRule.mAttributeResults.get(length - 1));
        assertEquals("ResizeType.AUTO_EXPANSION should be recorded once.", 1,
                histogramExpansion.getDelta());
        PartialCustomTabTestRule.waitForAnimationToFinish();
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void fixedHeightReactsTosoftKeyboard() {
        configureStatusBarHeightForR();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        assertTabIsAtInitialPos(getWindowAttributes());

        strategy.onShowSoftInput(() -> {});
        PartialCustomTabTestRule.waitForAnimationToFinish();
        // assertTabBelowStatusBar instead of assertTabIsFullHeight since
        // the height in mock is configured to return the device height minus
        // both navbar + status on R, which is more correct. By default on
        // other builds, status bar height was zero, thus ignored as it was
        // insignificant for tests.
        assertTabBelowStatusBar(getWindowAttributes());

        strategy.onImeStateChanged(/*imeVisible=*/true);
        assertTabBelowStatusBar(getWindowAttributes());

        strategy.onImeStateChanged(/*imeVisible=*/false);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.Q)
    public void fixedHeightReactsToSoftKeyboardBelowR() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        assertTabIsAtInitialPos(getWindowAttributes());

        strategy.onShowSoftInput(() -> {});
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsFullHeight(getWindowAttributes());

        strategy.onImeStateChanged(/*imeVisible=*/true);
        assertTabIsFullHeight(getWindowAttributes());

        strategy.onImeStateChanged(/*imeVisible=*/false);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void fixedHeightRotateWithSoftKeyboard() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        assertTabIsAtInitialPos(getWindowAttributes());

        strategy.onShowSoftInput(() -> {});
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsFullHeight(getWindowAttributes());

        mPCCTTestRule.configLandscapeMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        mPCCTTestRule.configPortraitMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void fixedHeightRotateDuringFindInPage() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        strategy.setToolbarColorForTesting(PCCT_TOOLBAR_COLOR);
        doReturn(FIND_TOOLBAR_COLOR)
                .when(mPCCTTestRule.mResources)
                .getColor(eq(R.color.find_in_page_background_color));
        doReturn(mPCCTTestRule.mDragBarBackground).when(mPCCTTestRule.mDragBar).getBackground();
        assertTabIsAtInitialPos(getWindowAttributes());

        strategy.onFindToolbarShown();
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsFullHeight(getWindowAttributes());

        mPCCTTestRule.configLandscapeMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        mPCCTTestRule.configPortraitMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        // For fixed-height mode, move the tab back to initial height if the device was
        // rotated while the tab was temporarily full-height due to Find-in-page feature
        // expanding it automatically.
        strategy.onFindToolbarHidden();
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void moveUpFixedHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();

        // Try to drag up and verify that the location does not change.
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 250);
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void moveUpFixedHeightWithFling() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();

        // Drag down and then fling up hard, verify that the tab doesn't go up to the top.
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT + 100);
        strategy.onDragEnd(-FULL_HEIGHT * 2); // Mighty up-flinging
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void moveDownFixedHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Try to drag down and check that it returns to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void moveDownToDismissFixedHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        final boolean[] closed = {false};
        handleStrategy.setCloseClickHandler(() -> closed[0] = true);

        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("Close click handler should be called.", closed[0]);
    }

    @Test
    public void dragHandlebarInvisibleFixedHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(1, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        verify(mPCCTTestRule.mDragHandlebar).setVisibility(View.GONE);
    }

    @Test
    public void invokeResizeCallbackExpansion() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals("mPCCTTestRule.mAttributeResults should have exactly 1 element.", 1,
                mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        int expected = PartialCustomTabBottomSheetStrategy.ResizeType.MANUAL_EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType2", expected);

        // Drag to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        // invokeResizeCallback() should have been called and MANUAL_EXPANSION logged once.
        assertEquals("ResizeType.MANUAL_EXPANSION should be recorded once.", 1,
                histogramExpansion.getDelta());
    }

    @Test
    public void invokeResizeCallbackMinimization() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals("mPCCTTestRule.mAttributeResults should have exactly 1 element.", 1,
                mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top so it can be minimized in the next step.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        int expected = PartialCustomTabBottomSheetStrategy.ResizeType.MANUAL_MINIMIZATION;
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
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Slide back to the initial height -> no resize happens.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1400));
        verify(mPCCTTestRule.mOnResizedCallback, never()).onResized(anyInt(), anyInt());

        // Drag to the top -> resized.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 500));
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
        clearInvocations(mPCCTTestRule.mOnResizedCallback);

        // Slide back to the top -> no resize happens.
        assertTabIsFullHeight(dragTab(handleStrategy, 50, 100, 150));
        verify(mPCCTTestRule.mOnResizedCallback, never()).onResized(anyInt(), anyInt());

        // Drag to the initial height -> resized.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
    }

    @Test
    public void callbackUponRotation() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);

        mPCCTTestRule.configLandscapeMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(DEVICE_WIDTH), eq(DEVICE_HEIGHT));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);

        mPCCTTestRule.configPortraitMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
    }

    @Test
    public void verifyNavigationBarHeightInMultiWindowMode() {
        mPCCTTestRule.mMetrics = new DisplayMetrics();
        mPCCTTestRule.mMetrics.widthPixels = DEVICE_WIDTH;
        mPCCTTestRule.mMetrics.heightPixels = MULTIWINDOW_HEIGHT;
        doAnswer(invocation -> {
            DisplayMetrics displayMetrics = invocation.getArgument(0);
            displayMetrics.setTo(mPCCTTestRule.mMetrics);
            return null;
        })
                .when(mPCCTTestRule.mDisplay)
                .getMetrics(any(DisplayMetrics.class));
        when(mPCCTTestRule.mContentFrame.getHeight()).thenReturn(MULTIWINDOW_HEIGHT);
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        assertEquals(0, strategy.getNavbarHeightForTesting());
    }

    @Test
    public void adjustWidthInLandscapeMode() {
        mPCCTTestRule.configLandscapeMode(Surface.ROTATION_90);
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        WindowManager.LayoutParams attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);

        mPCCTTestRule.configPortraitMode();
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);

        mPCCTTestRule.configLandscapeMode(Surface.ROTATION_270);
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        attrs = getWindowAttributes();
        assertEquals(WindowManager.LayoutParams.MATCH_PARENT, attrs.width);
    }

    @Test
    public void enterAndExitHtmlFullscreen() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        assertTrue(getWindowAttributes().isFullscreen());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(DEVICE_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertFalse(getWindowAttributes().isFullscreen());
        assertEquals(height, getWindowAttributes().height);
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), anyInt());
    }

    @Test
    public void fullscreenInLandscapeMode() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        int height = getWindowAttributes().height;

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals(0, getWindowAttributes().y);
    }

    @Test
    public void rotateAcrossFullscreenMode() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        int height = getWindowAttributes().height;

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        strategy.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void dragToTheSameInitialY() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals("mPCCTTestRule.mAttributeResults should have exactly 1 element.", 1,
                mPCCTTestRule.mAttributeResults.size());

        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag tab slightly but actionDown and actionUp will be performed at the same Y.
        // The tab should remain open.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1500));
    }

    @Test
    public void dragBarMatchesFindToolbarInColor() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        strategy.setToolbarColorForTesting(PCCT_TOOLBAR_COLOR);
        doReturn(FIND_TOOLBAR_COLOR)
                .when(mPCCTTestRule.mResources)
                .getColor(eq(R.color.find_in_page_background_color));
        doReturn(mPCCTTestRule.mDragBarBackground).when(mPCCTTestRule.mDragBar).getBackground();

        strategy.onFindToolbarShown();
        verify(mPCCTTestRule.mDragBarBackground).setColor(FIND_TOOLBAR_COLOR);

        strategy.onFindToolbarHidden();
        verify(mPCCTTestRule.mDragBarBackground).setColor(PCCT_TOOLBAR_COLOR);
    }

    @Test
    public void noTopShadowAtFullHeight() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        PartialCustomTabHandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertNotEquals("Top margin should be non-zero for the shadow", 0,
                mPCCTTestRule.mLayoutParams.topMargin);

        dragTab(handleStrategy, 1500, 1000, 500);
        assertEquals("There should be no top shadow at full height", 0,
                mPCCTTestRule.mLayoutParams.topMargin);
    }

    @Test
    public void expandToFullHeightOnFindInPage() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        doReturn(mPCCTTestRule.mDragBarBackground).when(mPCCTTestRule.mDragBar).getBackground();
        int expected = PartialCustomTabBottomSheetStrategy.ResizeType.AUTO_EXPANSION;
        HistogramDelta histogramExpansion = new HistogramDelta("CustomTabs.ResizeType2", expected);
        strategy.onFindToolbarShown();
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertTabIsFullHeight(getWindowAttributes());
        assertEquals("ResizeType.AUTO_EXPANSION should be recorded once.", 1,
                histogramExpansion.getDelta());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
        clearInvocations(mPCCTTestRule.mOnResizedCallback);

        expected = PartialCustomTabBottomSheetStrategy.ResizeType.AUTO_MINIMIZATION;
        HistogramDelta histogramMinimization =
                new HistogramDelta("CustomTabs.ResizeType2", expected);
        strategy.onFindToolbarHidden();
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertTabIsAtInitialPos(getWindowAttributes());
        assertEquals("ResizeType.AUTO_MINIMIZATION should be recorded once.", 1,
                histogramMinimization.getDelta());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
    }
}
