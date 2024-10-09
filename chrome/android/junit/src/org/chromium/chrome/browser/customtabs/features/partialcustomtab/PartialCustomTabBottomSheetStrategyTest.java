// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET_MAXIMIZED;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

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

import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBottomSheetStrategy.BOTTOM_SHEET_MAX_WIDTH_DP_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_HEIGHT_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.DEVICE_WIDTH;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.FULL_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.NAVBAR_HEIGHT;
import static org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTestRule.STATUS_BAR_HEIGHT;

import android.animation.Animator.AnimatorListener;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.ContentGestureListener.GestureState;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.HandleStrategy;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.components.embedder_support.view.ContentView;

import java.util.concurrent.TimeUnit;

/** Tests for {@link PartialCustomTabHandleStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {PartialCustomTabTestRule.ShadowSemanticColorUtils.class})
@EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabBottomSheetStrategyTest {
    @Rule public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private static final int INITIAL_HEIGHT = DEVICE_HEIGHT / 2 - NAVBAR_HEIGHT;

    private static final int FIND_TOOLBAR_COLOR = 3755;
    private static final int PCCT_TOOLBAR_COLOR = 12111;

    private boolean mFullscreen;

    private PartialCustomTabBottomSheetStrategy createPcctBackgroundDisabled() {
        BrowserServicesIntentDataProvider intentData = mPCCTTestRule.mIntentData;
        when(intentData.getInitialActivityHeight()).thenReturn(500);
        PartialCustomTabBottomSheetStrategy pcct =
                new PartialCustomTabBottomSheetStrategy(
                        mPCCTTestRule.mActivity,
                        mPCCTTestRule.mIntentData,
                        () -> mPCCTTestRule.mTouchEventProvider,
                        () -> mPCCTTestRule.mTab,
                        mPCCTTestRule.mOnResizedCallback,
                        mPCCTTestRule.mOnActivityLayoutCallback,
                        mPCCTTestRule.mActivityLifecycleDispatcher,
                        mPCCTTestRule.mFullscreenManager,
                        /* isTablet= */ false,
                        /* startMaximized= */ false,
                        mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(
                mPCCTTestRule.mNavbar,
                mPCCTTestRule.mSpinnerView,
                mPCCTTestRule.mSpinner,
                mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator,
                mPCCTTestRule.mHandleStrategyFactory);
        return pcct;
    }

    private PartialCustomTabBottomSheetStrategy createPcctAtHeight(int heightPx) {
        return createPcctAtHeight(heightPx, false);
    }

    private PartialCustomTabBottomSheetStrategy createPcctAtHeight(
            int heightPx, boolean isFixedHeight) {
        BrowserServicesIntentDataProvider intentData = mPCCTTestRule.mIntentData;
        when(intentData.getInitialActivityHeight()).thenReturn(heightPx);
        when(intentData.isPartialCustomTabFixedHeight()).thenReturn(isFixedHeight);
        when(intentData.canInteractWithBackground()).thenReturn(true);
        PartialCustomTabBottomSheetStrategy pcct =
                new PartialCustomTabBottomSheetStrategy(
                        mPCCTTestRule.mActivity,
                        mPCCTTestRule.mIntentData,
                        () -> mPCCTTestRule.mTouchEventProvider,
                        () -> mPCCTTestRule.mTab,
                        mPCCTTestRule.mOnResizedCallback,
                        mPCCTTestRule.mOnActivityLayoutCallback,
                        mPCCTTestRule.mActivityLifecycleDispatcher,
                        mPCCTTestRule.mFullscreenManager,
                        /* isTablet= */ false,
                        /* startMaxmized= */ false,
                        mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(
                mPCCTTestRule.mNavbar,
                mPCCTTestRule.mSpinnerView,
                mPCCTTestRule.mSpinner,
                mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator,
                mPCCTTestRule.mHandleStrategyFactory);
        return pcct;
    }

    @Test
    public void create_heightIsCappedToHalfOfDeviceHeight() {
        createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());
    }

    @Test
    public void create_largeInitialHeight() {
        createPcctAtHeight(5000);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsFullHeight(mPCCTTestRule.getWindowAttributes());
    }

    @Test
    public void create_heightIsCappedToDeviceHeight() {
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsFullHeight(mPCCTTestRule.getWindowAttributes());
    }

    private void doTestHeightWithStatusBar() {
        when(mPCCTTestRule.mContentFrame.getHeight())
                .thenReturn(DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT);
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        mPCCTTestRule.verifyWindowFlagsSet();
        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
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
        assertEquals(
                DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT,
                mPCCTTestRule.mAttributeResults.get(0).height);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void create_maxHeightWithStatusBar_landscape_Q() {
        configureStatusBarHeightForQ();
        mPCCTTestRule.configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(
                DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT,
                mPCCTTestRule.mAttributeResults.get(0).height);
    }

    @Test
    public void create_landscapeOrientation() {
        int pcctHeight = 800;
        mPCCTTestRule.configLandscapeMode();
        createPcctAtHeight(pcctHeight);
        mPCCTTestRule.verifyWindowFlagsSet();

        // Full height when in landscape mode.
        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertEquals(pcctHeight, mPCCTTestRule.getWindowAttributes().height);
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

    private static void actionDown(HandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_DOWN, ypos));
    }

    private static void actionMove(HandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_MOVE, ypos));
    }

    private static void actionUp(HandleStrategy strategy, long ts, int ypos) {
        strategy.onTouchEvent(event(ts, MotionEvent.ACTION_UP, ypos));
    }

    /**
     * Simulate dragging the tab and lifting the finger at the end.
     * @param handleStrategy {@link PartialCustomTabHandleStrategy} object.
     * @param ypos Series of y positions simulating the events.
     * @return Window attributes after the dragging finishes.
     */
    private WindowManager.LayoutParams dragTab(HandleStrategy handleStrategy, int... ypos) {
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

    private void assertMotionEventIgnored(HandleStrategy handleStrategy) {
        assertFalse(
                handleStrategy.onInterceptTouchEvent(
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
        doAnswer(
                        invocation -> {
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
        doReturn(
                        new Rect(
                                0,
                                0,
                                mPCCTTestRule.mRealMetrics.widthPixels,
                                mPCCTTestRule.mRealMetrics.heightPixels))
                .when(windowMetric)
                .getBounds();
        doReturn(Insets.of(0, STATUS_BAR_HEIGHT, 0, 0))
                .when(windowInsets)
                .getInsets(eq(WindowInsets.Type.statusBars()));
        doReturn(Insets.of(0, 0, 0, NAVBAR_HEIGHT))
                .when(windowInsets)
                .getInsets(eq(WindowInsets.Type.navigationBars()));
        doReturn(Insets.of(0, 0, 0, NAVBAR_HEIGHT))
                .when(windowInsets)
                .getInsets(
                        eq(WindowInsets.Type.navigationBars() | WindowInsets.Type.displayCutout()));
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
    public void setTitleWhenLaunched() {
        final String title = "BottomSheet";
        var coordinator = mPCCTTestRule.mCoordinatorLayout;
        doReturn(false).when(coordinator).isAttachedToWindow();
        doReturn(title).when(mPCCTTestRule.mResources).getString(anyInt());
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        var listener = mPCCTTestRule.mAttachStateChangeListener;
        verify(coordinator).addOnAttachStateChangeListener(listener.capture());
        listener.getValue().onViewAttachedToWindow(null);
        verify(mPCCTTestRule.mWindow).setTitle(eq(title));
        verify(coordinator).removeOnAttachStateChangeListener(listener.getValue());
        clearInvocations(coordinator);

        // Once attached, the title is not set again.
        doReturn(true).when(coordinator).isAttachedToWindow();
        strategy.onPostInflationStartup();
        verify(coordinator, never()).addOnAttachStateChangeListener(any());
    }

    @Test
    public void moveFromTop() {
        // Drag to the top
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Shake the tab from the initial position slightly -> back to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1600));
    }

    @Test
    public void moveUp_landscapeOrientationUnresizable() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveUp_multiwindowModeUnresizable() {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertMotionEventIgnored(handleStrategy);
    }

    @Test
    public void moveDownToDismiss() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        final boolean[] finishRunnable = {false};
        strategy.handleCloseAnimation(() -> finishRunnable[0] = true);
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        dragTab(handleStrategy, INITIAL_HEIGHT, DEVICE_HEIGHT - 400);
        assertTrue("FinnishRunnable should be called.", finishRunnable[0]);
    }

    @Test
    public void showSpinnerOnDragUpOnly() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        verify(mPCCTTestRule.mWindow).addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        verify(mPCCTTestRule.mWindow).clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        verify(mPCCTTestRule.mSpinnerView).setVisibility(View.VISIBLE);
        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.VISIBLE);
        clearInvocations(mPCCTTestRule.mSpinnerView);

        // Verify the spinner remains invisible after the tab reaches the top.
        int topY = strategy.getFullyExpandedY();
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

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        when(mPCCTTestRule.mSpinnerView.getVisibility()).thenReturn(View.GONE);

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));

        strategy.onShowSoftInput(CallbackUtils.emptyRunnable());
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        final int length = mPCCTTestRule.mAttributeResults.size();
        assertTrue(length > 1);

        // Verify that the tab expands to full height.
        assertTabIsFullHeight(mPCCTTestRule.mAttributeResults.get(length - 1));
        PartialCustomTabTestRule.waitForAnimationToFinish();
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void fixedHeightReactsTosoftKeyboard() {
        configureStatusBarHeightForR();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        assertTabIsAtInitialPos(getWindowAttributes());

        strategy.onShowSoftInput(CallbackUtils.emptyRunnable());
        PartialCustomTabTestRule.waitForAnimationToFinish();
        // assertTabBelowStatusBar instead of assertTabIsFullHeight since
        // the height in mock is configured to return the device height minus
        // both navbar + status on R, which is more correct. By default on
        // other builds, status bar height was zero, thus ignored as it was
        // insignificant for tests.
        assertTabBelowStatusBar(getWindowAttributes());

        strategy.onImeStateChanged(/* imeVisible= */ true);
        assertTabBelowStatusBar(getWindowAttributes());

        strategy.onImeStateChanged(/* imeVisible= */ false);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void fixedHeightReactsToSoftKeyboardBelowR() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        assertTabIsAtInitialPos(getWindowAttributes());

        strategy.onShowSoftInput(CallbackUtils.emptyRunnable());
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsFullHeight(getWindowAttributes());

        strategy.onImeStateChanged(/* imeVisible= */ true);
        assertTabIsFullHeight(getWindowAttributes());

        strategy.onImeStateChanged(/* imeVisible= */ false);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTabIsAtInitialPos(getWindowAttributes());
    }

    @Test
    public void moveUpFixedHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Try to drag down and check that it returns to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void dragHandlebarInvisibleFixedHeight() {
        createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(2, mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        verify(mPCCTTestRule.mDragHandlebar).setVisibility(View.GONE);
    }

    @Test
    public void invokeResizeCallbackExpansion() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "mPCCTTestRule.mAttributeResults should have exactly 2 elements, one for "
                        + "setting the height and one for setting the width.",
                2,
                mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        // invokeResizeCallback() should have been called and MANUAL_EXPANSION logged once.
    }

    @Test
    public void invokeResizeCallbackMinimization() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "mPCCTTestRule.mAttributeResults should have exactly 2 elements, one for "
                        + "setting the height and one for setting the width.",
                2,
                mPCCTTestRule.mAttributeResults.size());
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag to the top so it can be minimized in the next step.
        assertTabIsFullHeight(dragTab(handleStrategy, 1500, 1000, 0));

        // Drag down enough -> slide to the initial position.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 50, 650, 1300));

        // invokeResizeCallback() should have been called and MANUAL_MINIMIZATION logged once.
    }

    @Test
    public void callbackWhenHeightResized() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        assertTabIsAtInitialPos(mPCCTTestRule.mAttributeResults.get(0));
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

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
    public void verifyNavigationBarHeightInMultiWindowMode() {
        mPCCTTestRule.setupDisplayMetricsInMultiWindowMode();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        assertEquals(0, strategy.getNavbarHeightForTesting());
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void enterAndExitHtmlFullscreen() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        strategy.createHandleStrategyForTesting();
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        assertFalse(getWindowAttributes().isFullscreen());
        int height = getWindowAttributes().height;
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue(getWindowAttributes().isFullscreen());
        assertEquals("Shadow should be removed.", 0, strategy.getShadowOffsetForTesting());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(DEVICE_HEIGHT), eq(DEVICE_WIDTH));
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mFullscreen = false;
        strategy.onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertFalse(getWindowAttributes().isFullscreen());
        assertEquals(height, getWindowAttributes().height);
        assertNotEquals("Shadow should be restored.", 0, strategy.getShadowOffsetForTesting());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(height), anyInt());
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(DEVICE_HEIGHT - INITIAL_HEIGHT - NAVBAR_HEIGHT),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void dragToTheSameInitialY() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "mPCCTTestRule.mAttributeResults should have exactly 2 elements, one for "
                        + "setting the height and one for setting the width.",
                2,
                mPCCTTestRule.mAttributeResults.size());

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Drag tab slightly but actionDown and actionUp will be performed at the same Y.
        // The tab should remain open.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1450, 1500));
    }

    @Test
    public void dragBarMatchesFindToolbarInColor() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        strategy.setToolbarColorForTesting(PCCT_TOOLBAR_COLOR);
        doReturn(FIND_TOOLBAR_COLOR)
                .when(mPCCTTestRule.mActivity)
                .getColor(eq(R.color.find_in_page_background_color));
        doReturn(mPCCTTestRule.mDragBarBackground).when(mPCCTTestRule.mDragBar).getBackground();

        strategy.onFindToolbarShown();
        verify(mPCCTTestRule.mDragBarBackground).setColor(FIND_TOOLBAR_COLOR);

        strategy.onFindToolbarHidden();
        verify(mPCCTTestRule.mDragBarBackground).setColor(PCCT_TOOLBAR_COLOR);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void noTopShadowAtFullHeight() {
        doReturn(47)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertNotEquals(
                "Top margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);

        dragTab(handleStrategy, 1500, 1000, 500);
        assertEquals(
                "There should be no top shadow at full height",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void sideShadowsWith900dpBottomSheet() {
        doReturn(8)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configPortraitMode();
        mPCCTTestRule.mRealMetrics.widthPixels = 700;
        mPCCTTestRule.mDisplaySize.x = 700;
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertEquals(
                "Left margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
        assertEquals(
                "Right margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);

        mPCCTTestRule.configLandscapeMode();
        strategy = createPcctAtHeight(800);
        handleStrategy = strategy.createHandleStrategyForTesting();

        assertNotEquals(
                "Left margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
        assertNotEquals(
                "Right margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);

        // Drag to the top
        assertEquals(DEVICE_HEIGHT_LANDSCAPE, dragTab(handleStrategy, 1500, 1000, 0).height);

        assertNotEquals(
                "Left margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
        assertNotEquals(
                "Right margin should be non-zero for the shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Test
    public void noTopShadowFullHeightBottomSheet() {
        doReturn(8)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));

        mPCCTTestRule.configPortraitMode();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(3000);
        strategy.createHandleStrategyForTesting();
        assertEquals(
                "Top margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);

        mPCCTTestRule.configLandscapeMode();
        strategy = createPcctAtHeight(3000);
        strategy.createHandleStrategyForTesting();
        assertEquals(
                "Top margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);
    }

    @Test
    public void largeDeviceInPortrait_screenWidth() {
        doReturn(8)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configPortraitMode();
        mPCCTTestRule.mRealMetrics.widthPixels = 6000;
        mPCCTTestRule.mRealMetrics.heightPixels = 9500;
        mPCCTTestRule.mDisplaySize.x = 6000;
        mPCCTTestRule.mDisplaySize.y = 9500;
        createPcctAtHeight(5000);
        assertEquals(
                "Left margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
        assertEquals(
                "Right margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Bottom sheet width should be the screen width", 6000, getWindowAttributes().width);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void largeDeviceInLandscape_900dpWidth() {
        doReturn(8)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configLandscapeMode();
        mPCCTTestRule.mRealMetrics.widthPixels = 9500;
        mPCCTTestRule.mRealMetrics.heightPixels = 6000;
        mPCCTTestRule.mDisplaySize.x = 6000;
        mPCCTTestRule.mDisplaySize.y = 9500;
        createPcctAtHeight(5000);
        assertNotEquals(
                "Left margin should not be zero because there is a shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
        assertNotEquals(
                "Right margin should not be zero because there is a shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Bottom sheet width should be 900dp",
                BOTTOM_SHEET_MAX_WIDTH_DP_LANDSCAPE * mPCCTTestRule.getDisplayDensity(),
                getWindowAttributes().width,
                0.01f);
    }

    @CommandLineFlags.Add({BaseSwitches.ENABLE_LOW_END_DEVICE_MODE})
    @Test
    public void useDividerLine_LowEndDevice() {
        doReturn(8)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configPortraitMode();
        createPcctAtHeight(1500);

        assertEquals(
                "Top margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);

        // 900 dp landscape bottom sheet
        mPCCTTestRule.configLandscapeMode();
        createPcctAtHeight(3000);
        assertEquals(
                "Right margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Left margin should not be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Config(sdk = Build.VERSION_CODES.P)
    @Test
    public void useDividerLine_OldOS() {
        doReturn(8)
                .when(mPCCTTestRule.mResources)
                .getDimensionPixelSize(eq(R.dimen.custom_tabs_shadow_offset));
        mPCCTTestRule.configPortraitMode();
        createPcctAtHeight(1500);

        assertEquals(
                "Top margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.topMargin);

        // 900 dp landscape bottom sheet
        mPCCTTestRule.configLandscapeMode();
        createPcctAtHeight(3000);
        assertEquals(
                "Right margin should be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.rightMargin);
        assertEquals(
                "Left margin should not be zero because there is no shadow",
                0,
                mPCCTTestRule.mLayoutParams.leftMargin);
    }

    @Test
    public void expandToFullHeightOnFindInPage() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        doReturn(mPCCTTestRule.mDragBarBackground).when(mPCCTTestRule.mDragBar).getBackground();
        strategy.onFindToolbarShown();
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertTabIsFullHeight(getWindowAttributes());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(FULL_HEIGHT), anyInt());
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(0),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        strategy.onFindToolbarHidden();
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertTabIsAtInitialPos(getWindowAttributes());
        verify(mPCCTTestRule.mOnResizedCallback).onResized(eq(INITIAL_HEIGHT), anyInt());
        clearInvocations(mPCCTTestRule.mOnResizedCallback);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        eq(0),
                        eq(DEVICE_HEIGHT - INITIAL_HEIGHT - NAVBAR_HEIGHT),
                        eq(DEVICE_WIDTH),
                        eq(DEVICE_HEIGHT - NAVBAR_HEIGHT),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void contentScrollMayResizeTab() {
        var intentData = mPCCTTestRule.mIntentData;
        when(intentData.contentScrollMayResizeTab()).thenReturn(true);
        ContentView contentView = Mockito.mock(ContentView.class);
        when(mPCCTTestRule.mTab.getContentView()).thenReturn(contentView);

        var strategy = createPcctAtHeight(500);
        GestureDetector detector = Mockito.mock(GestureDetector.class);
        ContentGestureListener listener = Mockito.mock(ContentGestureListener.class);
        strategy.setGestureObjectsForTesting(detector, listener);

        MotionEvent e = Mockito.mock(MotionEvent.class);
        when(e.getActionMasked()).thenReturn(MotionEvent.ACTION_DOWN);
        when(listener.getState()).thenReturn(GestureState.NONE);

        // At initial state (none) -> down event forwarded to GestureDetector
        assertFalse(strategy.onInterceptTouchEvent(e));
        verify(detector).onTouchEvent(e);
        clearInvocations(detector);

        strategy.onTouchEvent(e);
        verify(detector, never()).onTouchEvent(e);

        // At content-scroll state -> forward events to contentview
        when(listener.getState()).thenReturn(GestureState.SCROLL_CONTENT);
        when(e.getActionMasked()).thenReturn(MotionEvent.ACTION_MOVE);
        strategy.onTouchEvent(e);
        verify(detector).onTouchEvent(e);
        verify(contentView).onTouchEvent(e);

        clearInvocations(detector);
        clearInvocations(contentView);

        // Lift up finger -> release
        when(e.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        strategy.onTouchEvent(e);
        verify(detector).onTouchEvent(e);
        verify(contentView).onTouchEvent(e);
        verify(listener).doNonFlingRelease();
    }
}
