// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET_MAXIMIZED;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
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

import android.graphics.Insets;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowMetrics;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.ContentGestureListener.GestureState;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar.HandleStrategy;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link PartialCustomTabBottomSheetStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES})
public class PartialCustomTabBottomSheetStrategyTest {
    @Rule public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private static final int INITIAL_HEIGHT = DEVICE_HEIGHT / 2 - NAVBAR_HEIGHT;

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
                mPCCTTestRule.mSpinnerView,
                mPCCTTestRule.mSpinner,
                mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator,
                mPCCTTestRule.mHandleStrategyFactory);
        return pcct;
    }

    private PartialCustomTabBottomSheetStrategy createEphemeralPcctAtHeight(int heightPx) {
        return createPcct(heightPx, false, true);
    }

    private PartialCustomTabBottomSheetStrategy createPcctAtHeight(int heightPx) {
        return createPcct(heightPx, false, false);
    }

    private PartialCustomTabBottomSheetStrategy createPcctAtHeight(
            int heightPx, boolean isFixedHeight) {
        return createPcct(heightPx, isFixedHeight, false);
    }

    private PartialCustomTabBottomSheetStrategy createPcct(
            int heightPx, boolean isFixedHeight, boolean isEphemeral) {
        BrowserServicesIntentDataProvider intentData = mPCCTTestRule.mIntentData;
        when(intentData.getInitialActivityHeight()).thenReturn(heightPx);
        when(intentData.isPartialCustomTabFixedHeight()).thenReturn(isFixedHeight);
        when(intentData.canInteractWithBackground()).thenReturn(true);

        if (isEphemeral) {
            when(intentData.getCustomTabMode()).thenReturn(CustomTabProfileType.EPHEMERAL);
        }

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

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());
    }

    @Test
    public void create_largeInitialHeight() {
        createPcctAtHeight(5000);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsFullHeight(mPCCTTestRule.getWindowAttributes());
    }

    @Test
    public void create_heightIsCappedToDeviceHeight() {
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsFullHeight(mPCCTTestRule.getWindowAttributes());
    }

    private void doTestHeightWithStatusBar() {
        mPCCTTestRule.setContentFrameHeight(DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT);
        createPcctAtHeight(DEVICE_HEIGHT + 100);
        mPCCTTestRule.verifyWindowFlagsSet();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_R() {
        configureStatusBarHeightForR();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mPCCTTestRule.getWindowAttributes());
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MIN_SDK)
    public void create_maxHeightWithStatusBar_Q() {
        mPCCTTestRule.configureStatusBarHeightForQ();
        doTestHeightWithStatusBar();
        assertTabBelowStatusBar(mPCCTTestRule.getWindowAttributes());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void create_maxHeightWithStatusBar_landscape_R() {
        configureStatusBarHeightForR();
        mPCCTTestRule.configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(
                DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT,
                mPCCTTestRule.getWindowAttributes().height);
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MIN_SDK)
    public void create_maxHeightWithStatusBar_landscape_Q() {
        mPCCTTestRule.configureStatusBarHeightForQ();
        mPCCTTestRule.configLandscapeMode();
        doTestHeightWithStatusBar();
        assertEquals(
                DEVICE_HEIGHT - NAVBAR_HEIGHT - STATUS_BAR_HEIGHT,
                mPCCTTestRule.getWindowAttributes().height);
    }

    @Test
    public void create_landscapeOrientation() {
        int pcctHeight = 800;
        mPCCTTestRule.configLandscapeMode();
        createPcctAtHeight(pcctHeight);
        mPCCTTestRule.verifyWindowFlagsSet();

        // Full height when in landscape mode.
        assertEquals(pcctHeight, mPCCTTestRule.getWindowAttributes().height);
    }

    @Test
    public void create_backgroundAppDisabledPortrait() {
        // Start with the opposite flags so that the asserts below are meaningful.
        Window window = mPCCTTestRule.mActivity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        createPcctBackgroundDisabled();

        assertTrue(mPCCTTestRule.hasWindowFlag(WindowManager.LayoutParams.FLAG_DIM_BEHIND));
        assertFalse(mPCCTTestRule.hasWindowFlag(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL));
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
     *
     * @param handleStrategy {@link PartialCustomTabHandleStrategy} object.
     * @param ypos Series of y positions simulating the events.
     * @return Window attributes after the dragging finishes.
     */
    private WindowManager.LayoutParams dragTab(HandleStrategy handleStrategy, int... ypos) {
        int npos = ypos.length;
        assertThat(npos).isAtLeast(2);
        long timestamp = SystemClock.uptimeMillis();

        // ACTION_DOWN -> ACTION_MOVE * (npos-1) -> ACTION_UP
        actionDown(handleStrategy, timestamp, ypos[0]);
        for (int i = 1; i < npos; ++i) actionMove(handleStrategy, timestamp, ypos[i]);
        actionUp(handleStrategy, timestamp, ypos[npos - 1]);

        // Wait animation to finish.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        return mPCCTTestRule.getWindowAttributes();
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

    private WindowManager.LayoutParams getWindowAttributes() {
        return mPCCTTestRule.getWindowAttributes();
    }

    private void configureStatusBarHeightForR() {
        // Setup for R+
        WindowInsets windowInsets =
                new WindowInsets.Builder()
                        .setInsets(
                                WindowInsets.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_HEIGHT, 0, 0))
                        .setInsets(
                                WindowInsets.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAVBAR_HEIGHT))
                        .build();
        WindowMetrics windowMetric =
                new WindowMetrics(new Rect(0, 0, DEVICE_WIDTH, DEVICE_HEIGHT), windowInsets);
        doReturn(windowMetric).when(mPCCTTestRule.mWindowManager).getCurrentWindowMetrics();
    }

    @Test
    public void setTitleWhenLaunched() {
        var activity = mPCCTTestRule.mActivity;
        final String title =
                activity.getString(R.string.accessibility_partial_custom_tab_bottom_sheet);
        var coordinator = mPCCTTestRule.mCoordinatorLayout;
        assertFalse(coordinator.isAttachedToWindow());
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        Window window = activity.getWindow();
        var shadowWindow = shadowOf(window);
        assertNotEquals(title, shadowWindow.getTitle());

        // Attach the window. Uses the WindowManager service since getWindowManager() is mocked.
        View decorView = window.getDecorView();
        WindowManager windowManager = activity.getSystemService(WindowManager.class);
        windowManager.addView(decorView, new WindowManager.LayoutParams());
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(coordinator.isAttachedToWindow());
        assertEquals(title, shadowWindow.getTitle());

        // Once attached, the title is not set again.
        window.setTitle("Other");
        strategy.onPostInflationStartup();
        ViewGroup parent = (ViewGroup) coordinator.getParent();
        parent.removeView(coordinator);
        parent.addView(coordinator);
        assertEquals("Other", shadowWindow.getTitle());

        windowManager.removeViewImmediate(decorView);
    }

    @Test
    public void moveFromTop() {
        // Drag to the top
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

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

        mPCCTTestRule.verifyWindowFlagsSet();

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

        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        assertEquals(View.VISIBLE, mPCCTTestRule.mSpinnerView.getVisibility());

        actionUp(handleStrategy, timestamp, 1450);

        // Wait animation to finish.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(View.GONE, mPCCTTestRule.mSpinnerView.getVisibility());

        // Now the tab is full-height. Start dragging down.
        actionDown(handleStrategy, timestamp, 500);
        actionMove(handleStrategy, timestamp, 650);

        // Verify the spinner remained invisible.
        assertEquals(View.GONE, mPCCTTestRule.mSpinnerView.getVisibility());
    }

    @Test
    public void hideSpinnerWhenReachingFullHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, 1500);
        actionMove(handleStrategy, timestamp, 1450);

        // Verify the spinner is visible.
        assertEquals(View.VISIBLE, mPCCTTestRule.mSpinnerView.getVisibility());

        // Verify the spinner remains invisible after the tab reaches the top.
        int topY = strategy.getFullyExpandedY();
        actionMove(handleStrategy, timestamp, topY);
        idleUntilJustBeforeSpinnerTimeout();
        assertEquals(View.GONE, mPCCTTestRule.mSpinnerView.getVisibility());

        actionMove(handleStrategy, timestamp, topY + 200);
        assertEquals(View.GONE, mPCCTTestRule.mSpinnerView.getVisibility());
    }

    @Test
    public void hideSpinnerWhenDraggingDown() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        assertEquals(View.VISIBLE, mPCCTTestRule.mSpinnerView.getVisibility());

        // Drag below the initial height.
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT + 100);

        // Verify the spinner goes invisible.
        idleUntilJustBeforeSpinnerTimeout();
        assertEquals(View.GONE, mPCCTTestRule.mSpinnerView.getVisibility());
    }

    @Test
    public void hideSpinnerEarly() {
        // Test hiding spinner early (500ms after showing) when there is no glitch at
        // the end of draggin action.
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        long timestamp = SystemClock.uptimeMillis();
        actionDown(handleStrategy, timestamp, INITIAL_HEIGHT - 100);
        actionMove(handleStrategy, timestamp, INITIAL_HEIGHT - 150);

        // Verify the spinner is visible.
        assertEquals(View.VISIBLE, mPCCTTestRule.mSpinnerView.getVisibility());

        // Verify the spinner goes invisible after the specified timeout, but not before.
        idleUntilJustBeforeSpinnerTimeout();
        assertEquals(View.VISIBLE, mPCCTTestRule.mSpinnerView.getVisibility());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(View.GONE, mPCCTTestRule.mSpinnerView.getVisibility());
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

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        strategy.onShowSoftInput(CallbackUtils.emptyRunnable());
        PartialCustomTabTestRule.waitForAnimationToFinish();

        // Verify that the tab expands to full height.
        assertTabIsFullHeight(mPCCTTestRule.getWindowAttributes());
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

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();

        // Try to drag down and check that it returns to the initial height.
        assertTabIsAtInitialPos(dragTab(handleStrategy, 1500, 1550, 1600));
    }

    @Test
    public void dragHandlebarInvisibleFixedHeight() {
        createPcctAtHeight(500, true);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());

        assertEquals(View.GONE, mPCCTTestRule.mDragHandlebar.getVisibility());
    }

    @Test
    public void invokeResizeCallbackExpansion() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(500);
        mPCCTTestRule.verifyWindowFlagsSet();

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
        assertTabIsAtInitialPos(mPCCTTestRule.getWindowAttributes());
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

        strategy.setFullscreenSupplierForTesting(() -> mFullscreen);

        mFullscreen = true;
        strategy.onEnterFullscreen(null, null);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue(getWindowAttributes().isFullscreen());
        assertEquals("Shadow should be removed.", 0, strategy.getTopMarginForTesting());
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
        assertNotEquals("Shadow should be restored.", 0, strategy.getTopMarginForTesting());
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
        int findToolbarColor =
                mPCCTTestRule.mActivity.getColor(R.color.find_in_page_background_color);

        strategy.onFindToolbarShown();
        assertEquals(findToolbarColor, getDragBarColor());

        strategy.onFindToolbarHidden();
        assertEquals(PCCT_TOOLBAR_COLOR, getDragBarColor());
    }

    @Test
    public void noTopShadowAtFullHeight() {
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(800);
        HandleStrategy handleStrategy = strategy.createHandleStrategyForTesting();
        assertNotEquals(
                "Top shadow offset should be non-zero for the shadow",
                0,
                strategy.getShadowOffsetForTesting());

        dragTab(handleStrategy, 1500, 1000, 500);
        assertEquals(
                "There should be no top shadow at full height",
                0,
                strategy.getShadowOffsetForTesting());
    }

    @Test
    public void sideShadowsWith900dpBottomSheet() {
        mPCCTTestRule.configPortraitMode();
        var shadowDisplay = mPCCTTestRule.mShadowDisplay;
        shadowDisplay.setRealWidth(700);
        shadowDisplay.setWidth(700);
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
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(3000);
        strategy.createHandleStrategyForTesting();
        assertEquals(
                "Top shadow offset should be zero because there is no shadow",
                0,
                strategy.getShadowOffsetForTesting());

        mPCCTTestRule.configLandscapeMode();
        strategy = createPcctAtHeight(3000);
        strategy.createHandleStrategyForTesting();
        assertEquals(
                "Top shadow offset should be zero because there is no shadow",
                0,
                strategy.getShadowOffsetForTesting());
    }

    @Test
    public void largeDeviceInPortrait_screenWidth() {
        mPCCTTestRule.configPortraitMode();
        var shadowDisplay = mPCCTTestRule.mShadowDisplay;
        shadowDisplay.setRealWidth(6000);
        shadowDisplay.setRealHeight(9500);
        shadowDisplay.setWidth(6000);
        shadowDisplay.setHeight(9500);
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

    @Test
    public void largeDeviceInLandscape_900dpWidth() {
        mPCCTTestRule.configLandscapeMode();
        var shadowDisplay = mPCCTTestRule.mShadowDisplay;
        shadowDisplay.setRealWidth(9500);
        shadowDisplay.setRealHeight(6000);
        shadowDisplay.setWidth(6000);
        shadowDisplay.setHeight(9500);
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
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabBottomSheetStrategy strategy = createPcctAtHeight(1500);

        assertEquals(
                "Top shadow offset should be zero because there is no shadow",
                0,
                strategy.getShadowOffsetForTesting());

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
        TestContentView contentView =
                new TestContentView(ApplicationProvider.getApplicationContext());
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
        assertEquals(List.of(e), contentView.getTouchEvents());

        clearInvocations(detector);

        // Lift up finger -> release
        when(e.getActionMasked()).thenReturn(MotionEvent.ACTION_UP);
        strategy.onTouchEvent(e);
        verify(detector).onTouchEvent(e);
        assertEquals(List.of(e, e), contentView.getTouchEvents());
        verify(listener).doNonFlingRelease();
    }

    @Test
    @EnableFeatures("PCCTMinimumHeight:pcct_minimum_height_ratio/0.7")
    public void create_heightIsClampedToMinimumRatioOfDeviceHeightFromFeature() {
        // Scenario: Requested height is lower than the minimum defined by the feature's ratio.
        // Expected: The height is clamped UP to the ratio-defined minimum.
        final double minHeightRatio = 0.7;
        final int requestedHeight = 100; // A value lower than any minimum.

        final int ratioCalculatedHeightPx = (int) (DEVICE_HEIGHT * minHeightRatio);

        final int expectedMinContentHeightPx = ratioCalculatedHeightPx;
        final int expectedY = DEVICE_HEIGHT - expectedMinContentHeightPx;

        createEphemeralPcctAtHeight(requestedHeight);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Window y-position should be clamped up by the minimum height ratio of the device"
                        + " height.",
                expectedY,
                mPCCTTestRule.getWindowAttributes().y);
    }

    @Test
    @EnableFeatures("PCCTMinimumHeight:pcct_minimum_height_ratio/0.05")
    public void create_heightIsClampedToAbsoluteMinimumWhenRatioIsTooSmall() {
        // Scenario: The feature's ratio calculates a height smaller than the hardcoded minimum.
        // Expected: The height is clamped UP to the absolute minimum (220).
        final int requestedHeight = 100; // A value lower than any minimum.
        final int absoluteMinHeightDp = 220;

        final int expectedMinContentHeightPx =
                (int) (absoluteMinHeightDp * PartialCustomTabTestRule.DENSITY);
        final int expectedY = DEVICE_HEIGHT - expectedMinContentHeightPx;

        createEphemeralPcctAtHeight(requestedHeight);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Window y-position should be clamped up by the absolute minimum height.",
                expectedY,
                mPCCTTestRule.getWindowAttributes().y);
    }

    @Test
    @EnableFeatures("PCCTMinimumHeight:pcct_minimum_height_ratio/0.6")
    public void create_requestedHeightIsUsedWhenAboveFeatureMinimum() {
        final double minHeightRatio = 0.6;
        final int absoluteMinHeightDp = 220;

        final int absoluteMinHeightPx =
                (int) (absoluteMinHeightDp * PartialCustomTabTestRule.DENSITY);
        final int ratioCalculatedHeightPx = (int) (DEVICE_HEIGHT * minHeightRatio);

        final int featureMinContentHeightPx =
                Math.max(absoluteMinHeightPx, ratioCalculatedHeightPx);

        // Scenario: The requested height is valid and above the feature's minimum.
        // Expected: The requested height should be used without modification.
        final int requestedHeight = featureMinContentHeightPx + 200;
        final int expectedY = DEVICE_HEIGHT - requestedHeight;

        createEphemeralPcctAtHeight(requestedHeight);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Window y-position should correspond to the requested height when it's valid.",
                expectedY,
                mPCCTTestRule.getWindowAttributes().y);
    }

    @Test
    public void create_heightIsClampedToDefaultMinimumWhenFeatureIsDisabled() {
        // Scenario: The new feature is disabled.
        // Expected: The logic falls back to the default minimum (50% of screen height).
        final int requestedHeight = 100;

        // The default minimum height is 50% of device height.
        final int expectedDefaultMinHeight = (int) (DEVICE_HEIGHT * 0.5f);
        final int expectedY = DEVICE_HEIGHT - expectedDefaultMinHeight;

        createPcctAtHeight(requestedHeight);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Window y-position should be clamped by the default 50% minimum when feature is"
                        + " disabled.",
                expectedY, mPCCTTestRule.getWindowAttributes().y);
    }

    /**
     * Advances time enough for the spinner to fade out, but not enough to reach the spinner timeout
     * (which also hides it).
     */
    private static void idleUntilJustBeforeSpinnerTimeout() {
        shadowOf(Looper.getMainLooper())
                .idleFor(
                        PartialCustomTabBottomSheetStrategy.SPINNER_TIMEOUT_MS - 1,
                        TimeUnit.MILLISECONDS);
    }

    private int getDragBarColor() {
        var background = (GradientDrawable) mPCCTTestRule.mDragBar.getBackground();
        return background.getColor().getDefaultColor();
    }
}
