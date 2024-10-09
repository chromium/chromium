// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_SIDE_SHEET;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.os.Build;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager.SizeStrategyCreator;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link PartialCustomTabDisplayManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {PartialCustomTabTestRule.ShadowSemanticColorUtils.class})
@LooperMode(Mode.PAUSED)
public class PartialCustomTabDisplayManagerTest {
    private static final int BOTTOM_SHEET_MAX_WIDTH_DP = 900;

    private boolean mFullscreen;
    @Rule public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabDisplayManager createPcctDisplayManager() {
        return createPcctDisplayManager(800, 2000);
    }

    private PartialCustomTabDisplayManager createPcctDisplayManager(
            @Px int heightPx, @Px int widthPx) {
        return createPcctDisplayManager(
                heightPx, widthPx, 1850, ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW);
    }

    private PartialCustomTabDisplayManager createPcctDisplayManager(
            @Px int heightPx, @Px int widthPx, int breakPointDp, int decorationType) {
        BrowserServicesIntentDataProvider intentData = mPCCTTestRule.mIntentData;
        when(intentData.getInitialActivityHeight()).thenReturn(heightPx);
        when(intentData.getInitialActivityWidth()).thenReturn(widthPx);
        when(intentData.getActivityBreakPoint()).thenReturn(breakPointDp);
        when(intentData.canInteractWithBackground()).thenReturn(true);
        when(intentData.showSideSheetMaximizeButton()).thenReturn(true);
        when(intentData.getActivitySideSheetDecorationType()).thenReturn(decorationType);
        when(intentData.getSideSheetPosition()).thenReturn(ACTIVITY_SIDE_SHEET_POSITION_END);
        when(intentData.getSideSheetSlideInBehavior())
                .thenReturn(ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE);
        when(intentData.getActivitySideSheetRoundedCornersPosition())
                .thenReturn(ACTIVITY_SIDE_SHEET_ROUNDED_CORNERS_POSITION_NONE);
        PartialCustomTabDisplayManager displayManager =
                new PartialCustomTabDisplayManager(
                        mPCCTTestRule.mActivity,
                        mPCCTTestRule.mIntentData,
                        () -> mPCCTTestRule.mTouchEventProvider,
                        () -> mPCCTTestRule.mTab,
                        mPCCTTestRule.mOnResizedCallback,
                        mPCCTTestRule.mOnActivityLayoutCallback,
                        mPCCTTestRule.mActivityLifecycleDispatcher,
                        mPCCTTestRule.mFullscreenManager,
                        /* isTablet= */ false);
        var sizeStrategyCreator = displayManager.getSizeStrategyCreatorForTesting();
        SizeStrategyCreator testSizeStrategyCreator =
                (type, intentData0, maximized) -> {
                    var strategy = sizeStrategyCreator.createForType(type, intentData0, maximized);
                    strategy.setFullscreenSupplierForTesting(() -> mFullscreen);
                    strategy.setMockViewForTesting(
                            mPCCTTestRule.mCoordinatorLayout,
                            mPCCTTestRule.mToolbarView,
                            mPCCTTestRule.mToolbarCoordinator);
                    return strategy;
                };
        displayManager.setMocksForTesting(
                mPCCTTestRule.mCoordinatorLayout,
                mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator,
                mPCCTTestRule.mHandleStrategyFactory,
                testSizeStrategyCreator);
        return displayManager;
    }

    @Test
    public void create_FullSize_HeightNotSetWidthNotSet() {
        int expected = PartialCustomTabType.FULL_SIZE;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(0, 0);
        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabType.FULL_SIZE,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.FULL_SIZE should be recorded once");
    }

    @Test
    public void create_FullSize_WidthSetCompactDevice() {
        int expected = PartialCustomTabType.FULL_SIZE;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        mPCCTTestRule.configCompactDevice();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(0, 500);
        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabType.FULL_SIZE,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.FULL_SIZE should be recorded once");
    }

    @Test
    public void create_SideSheet_WidthSetHeightNot_BelowBreakpoint() {
        int expected = PartialCustomTabType.FULL_SIZE;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(0, 800);
        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabType.FULL_SIZE,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.FULL_SIZE should be recorded once");
    }

    @Test
    public void create_SideSheet_WidthSetHeightNot_AboveBreakpoint() {
        int expected = PartialCustomTabType.SIDE_SHEET;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        PartialCustomTabDisplayManager displayManager =
                createPcctDisplayManager(0, 2000, 840, ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW);
        assertEquals(
                "Breakpoint value is incorrect", 840, displayManager.getBreakPointDpForTesting());
        assertEquals(
                "Side-Sheet PCCT should be created",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.SIDE_SHEET should be recorded once");
    }

    @Test
    public void create_SideSheet_AboveBreakPoint() {
        int expected = PartialCustomTabType.SIDE_SHEET;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals(
                "Side-Sheet PCCT should be created",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.SIDE_SHEET should be recorded once");
    }

    @Test
    public void create_BottomSheet_HeightWidthSet_Compact() {
        int expected = PartialCustomTabType.BOTTOM_SHEET;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        mPCCTTestRule.configCompactDevice();
        PartialCustomTabDisplayManager displayManager =
                createPcctDisplayManager(350, 450, 400, ACTIVITY_SIDE_SHEET_DECORATION_TYPE_SHADOW);
        assertEquals(
                "Breakpoint value is incorrect", 600, displayManager.getBreakPointDpForTesting());
        assertEquals(
                "Bottom-Sheet PCCT should be created",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.BOTTOM_SHEET should be recorded once");
    }

    @Test
    public void create_BottomSheet_HeightSetWidthNot() {
        int expected = PartialCustomTabType.BOTTOM_SHEET;
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.PartialCustomTabType", expected);
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(800, 0);
        assertEquals(
                "Bottom-Sheet PCCT should be created",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        histogram.assertExpected("PartialCustomTabType.BOTTOM_SHEET should be recorded once");
    }

    @Test
    public void create_BottomSheet_BelowBreakPoint() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals(
                "Bottom-Sheet PCCT should be created",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_BottomSheetStrategy() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals(
                "Bottom-Sheet PCCT should be created",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_SideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals(
                "Side-Sheet PCCT should be created",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void transitionFromBottomSheetToSideSheetWhenOrientationChangedToLandscape() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals(
                "Side-Sheet should be the active strategy",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());

        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void transitionFromBottomSheetToSideSheetWhileSoftkeyboardIsOn() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        displayManager.onShowSoftInput(CallbackUtils.emptyRunnable());
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue(displayManager.getSizeStrategyForTesting().isMaximized());

        // Rotate while the soft keyboard is on.
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        // The destroyed bottom sheet should have its height state back to 'initial'.
        assertFalse(displayManager.getSizeStrategyForTesting().isMaximized());
    }

    @Test
    public void closeAnimationNotInvokedTwice() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();
        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        Runnable finish = Mockito.mock(Runnable.class);
        assertTrue("Close animation didn't run", displayManager.handleCloseAnimation(finish));
        Runnable finish2 = Mockito.mock(Runnable.class);
        assertFalse("Close animation shouldn't run", displayManager.handleCloseAnimation(finish2));

        mPCCTTestRule.configLandscapeMode();
        displayManager = createPcctDisplayManager();
        assertEquals(
                "Side-Sheet should be the active strategy",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        assertTrue("Close animation didn't run", displayManager.handleCloseAnimation(finish));
        assertFalse("Close animation shouldn't run", displayManager.handleCloseAnimation(finish2));

        displayManager = createPcctDisplayManager(0, 0);
        assertEquals(
                "Full-Size PCCT should be created",
                PartialCustomTabType.FULL_SIZE,
                displayManager.getActiveStrategyType());
        assertTrue("Close animation didn't run", displayManager.handleCloseAnimation(finish));
        assertFalse("Close animation shouldn't run", displayManager.handleCloseAnimation(finish2));
    }

    @Test
    public void
            transitionFromBottomSheetTo900dpBottomSheetWhenOrientationChangedToLandscape_andHeightSetWidthNot() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(800, 0);

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        // the density in this case is 1.0
        assertEquals(
                "Should be 900dp width bottom sheet",
                BOTTOM_SHEET_MAX_WIDTH_DP * mPCCTTestRule.getDisplayDensity(),
                mPCCTTestRule.getWindowAttributes().width,
                0.01f);
        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void transitionFromSideSheetToBottomSheetWhenOrientationChangedToPortrait() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals(
                "Side-Sheet should be the active strategy",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Config(sdk = Build.VERSION_CODES.Q)
    @Test
    public void transitionFromDividerSideSheetToBottomSheetWhenOrientationChangedToPortrait() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager =
                createPcctDisplayManager(
                        850, 2000, 1850, ACTIVITY_SIDE_SHEET_DECORATION_TYPE_DIVIDER);
        displayManager.onToolbarInitialized(
                mPCCTTestRule.mToolbarCoordinator, mPCCTTestRule.mToolbarView, 5);
        assertEquals(
                "Side-Sheet should be the active strategy",
                PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        verify(mPCCTTestRule.mCoordinatorLayout).setBackground(any(Drawable.class));
        mPCCTTestRule.configInsetDrawableBg();
        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());

        verify(mPCCTTestRule.mCoordinatorLayout).getPaddingRight();
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void dontTransitionIfOrientationDoesNotChange() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        verify(mPCCTTestRule.mOnActivityLayoutCallback, never())
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
    }

    @Test
    public void rotateInFullscreenMode() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        var attrs = mPCCTTestRule.getWindowAttributes();
        int height = attrs.height;
        int width = attrs.width;

        mFullscreen = true;
        displayManager
                .getSizeStrategyForTesting()
                .setFullscreenSupplierForTesting(() -> mFullscreen);

        displayManager.getSizeStrategyForTesting().onEnterFullscreen(null, null);
        assertTrue(mPCCTTestRule.getWindowAttributes().isFullscreen());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_FULL_SCREEN));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals(
                "Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
        // Bottom-sheet strategy is now in action. Verify its top margin is removed
        // for the correct fullscreen UI.
        assertEquals(
                "Top margin should be zero in fullscreen",
                0,
                displayManager.getSizeStrategyForTesting().getTopMarginForTesting());

        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        mFullscreen = false;
        displayManager.getSizeStrategyForTesting().onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        attrs = mPCCTTestRule.getWindowAttributes();
        assertFalse("Should not be in fulllscreen.", attrs.isFullscreen());
        assertEquals("Height should be restored.", height, attrs.height);
        assertEquals("Width should be restored.", width, attrs.width);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    @DisabledTest(message = "b/354044501")
    public void rotateInMaximizeMode() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        var attrs = mPCCTTestRule.getWindowAttributes();
        int height = attrs.height;
        int width = attrs.width;

        var sideSheetStrategy = displayManager.getSizeStrategyForTesting();
        ((PartialCustomTabSideSheetStrategy) sideSheetStrategy).toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();
        assertTrue("Should be in maximized state.", sideSheetStrategy.isMaximized());

        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        // Strategy is now set to bottom sheet. Verify it starts in initial-height mode regardless
        // of the previous strategy state.
        var bottomSheetStrategy = displayManager.getSizeStrategyForTesting();
        assertFalse(
                "Bottom sheet must start in initial height.", bottomSheetStrategy.isMaximized());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        sideSheetStrategy = displayManager.getSizeStrategyForTesting();

        // When coming back to SideSheet strategy, the last state should be restored.
        assertTrue("Side sheet must start in minimized state.", sideSheetStrategy.isMaximized());
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET_MAXIMIZED));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);

        ((PartialCustomTabSideSheetStrategy) sideSheetStrategy).toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        attrs = mPCCTTestRule.getWindowAttributes();
        assertFalse("Should not be in maximized state.", sideSheetStrategy.isMaximized());
        assertEquals("Height should be restored.", height, attrs.height);
        assertEquals("Width should be restored.", width, attrs.width);
        verify(mPCCTTestRule.mOnActivityLayoutCallback)
                .onActivityLayout(
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        eq(ACTIVITY_LAYOUT_STATE_SIDE_SHEET));
        clearInvocations(mPCCTTestRule.mOnActivityLayoutCallback);
    }

    @Test
    public void calculatePartialCustomTabTypePermutations() {
        int initWidth = 0;
        int initHeight = 0;
        Supplier<Integer> displayWidthDp = null;
        int breakPointDp = 0;

        // Multi-window mode -> FULL
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        assertEquals(
                "Type should be FULL_SIZE",
                PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));

        // Zero initial width/height -> FULL
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
        assertEquals(
                "Type should be FULL_SIZE",
                PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));

        // Non-zero height -> BOTTOM_SHEET
        initWidth = 0;
        initHeight = 500;
        assertEquals(
                "Type should be BOTTOM_SHEET",
                PartialCustomTabType.BOTTOM_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));

        // Non-zero width -> either FULL_SIZE or SIDE_SHEET
        initWidth = 500;
        initHeight = 0;
        displayWidthDp = () -> 1000;
        breakPointDp = 1200;
        assertEquals(
                "Type should be FULL_SIZE",
                PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));

        breakPointDp = 800;
        assertEquals(
                "Type should be SIDE_SHEET",
                PartialCustomTabType.SIDE_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));

        // Non-zero width/height -> either SIDE_SHEET or BOTTOM_SHEET
        initWidth = 300;
        initHeight = 500;
        displayWidthDp = () -> 1000;
        breakPointDp = 400;
        assertEquals(
                "Type should be SIDE_SHEET",
                PartialCustomTabType.SIDE_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));

        initWidth = 300;
        initHeight = 500;
        displayWidthDp = () -> 1000;
        breakPointDp = 1200;
        assertEquals(
                "Type should be BOTTOM_SHEET",
                PartialCustomTabType.BOTTOM_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp));
    }

    @Test
    public void startAnimationOverride() {
        int defId = 42;
        var provider = Mockito.mock(BrowserServicesIntentDataProvider.class);
        when(provider.getAnimationEnterRes()).thenReturn(defId);

        // FULL_SIZE -> slide up
        assertEquals(
                R.anim.slide_in_up,
                PartialCustomTabDisplayManager.getStartAnimationOverride(null, provider, defId));

        // BOTTOM_SHEET -> slide up
        when(provider.getInitialActivityHeight()).thenReturn(1000);
        assertEquals(
                R.anim.slide_in_up,
                PartialCustomTabDisplayManager.getStartAnimationOverride(null, provider, defId));

        // SIDE_SHEET -> slide from side
        when(provider.getSideSheetPosition()).thenReturn(ACTIVITY_SIDE_SHEET_POSITION_END);
        when(provider.getInitialActivityWidth()).thenReturn(1000);
        when(provider.getActivityBreakPoint()).thenReturn(500);

        Activity act = mPCCTTestRule.mActivity;
        when(provider.getSideSheetSlideInBehavior())
                .thenReturn(ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE);
        assertEquals(
                R.anim.slide_in_right,
                PartialCustomTabDisplayManager.getStartAnimationOverride(act, provider, defId));

        LocalizationUtils.setRtlForTesting(true);
        assertEquals(
                R.anim.slide_in_left,
                PartialCustomTabDisplayManager.getStartAnimationOverride(act, provider, defId));

        // BOTTOM_SHEET -> slide up (with height and width set, compact portrait)
        mPCCTTestRule.configCompactDevice_Portrait();
        when(provider.getInitialActivityWidth()).thenReturn(500);
        when(provider.getActivityBreakPoint()).thenReturn(50);
        assertEquals(
                R.anim.slide_in_up,
                PartialCustomTabDisplayManager.getStartAnimationOverride(act, provider, defId));
    }
}
