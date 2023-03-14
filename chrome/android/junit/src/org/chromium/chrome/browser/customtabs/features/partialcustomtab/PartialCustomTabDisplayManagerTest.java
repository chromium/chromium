// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_POSITION_END;
import static org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE;

import android.app.Activity;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager.SizeStrategyCreator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link PartialCustomTabDisplayManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET,
        ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET_FOR_THIRD_PARTIES})
public class PartialCustomTabDisplayManagerTest {
    private static final int BOTTOM_SHEET_MAX_WIDTH_DP = 900;

    private boolean mFullscreen;
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabDisplayManager createPcctDisplayManager() {
        return createPcctDisplayManager(800, 2000);
    }

    private PartialCustomTabDisplayManager createPcctDisplayManager(
            @Px int heightPx, @Px int widthPx) {
        return createPcctDisplayManager(heightPx, widthPx, 1850);
    }

    private PartialCustomTabDisplayManager createPcctDisplayManager(
            @Px int heightPx, @Px int widthPx, int breakPointDp) {
        PartialCustomTabDisplayManager displayManager = new PartialCustomTabDisplayManager(
                mPCCTTestRule.mActivity, heightPx, widthPx, breakPointDp, false,
                mPCCTTestRule.mOnResizedCallback, mPCCTTestRule.mActivityLifecycleDispatcher,
                mPCCTTestRule.mFullscreenManager, false, true, /*showMaximizeButton=*/true, 0,
                ACTIVITY_SIDE_SHEET_POSITION_END, ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE);
        var sizeStrategyCreator = displayManager.getSizeStrategyCreatorForTesting();
        SizeStrategyCreator testSizeStrategyCreator =
                (type, maximized, sideSheetPosition, sideSheetAnimation) -> {
            var strategy = sizeStrategyCreator.createForType(
                    type, maximized, sideSheetPosition, sideSheetAnimation);
            strategy.setFullscreenSupplierForTesting(() -> mFullscreen);
            strategy.setMockViewForTesting(mPCCTTestRule.mCoordinatorLayout,
                    mPCCTTestRule.mToolbarView, mPCCTTestRule.mToolbarCoordinator);
            return strategy;
        };
        displayManager.setMocksForTesting(mPCCTTestRule.mCoordinatorLayout,
                mPCCTTestRule.mToolbarView, mPCCTTestRule.mToolbarCoordinator,
                mPCCTTestRule.mHandleStrategyFactory, testSizeStrategyCreator);
        return displayManager;
    }

    @Test
    public void create_FullSize_HeightNotSetWidthNotSet() {
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(0, 0);
        assertEquals("Full-Size PCCT should be created", PartialCustomTabType.FULL_SIZE,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_SideSheet_WidthSetHeightNot_BelowBreakpoint() {
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(0, 100);
        assertEquals("Full-Size PCCT should be created", PartialCustomTabType.FULL_SIZE,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_SideSheet_WidthSetHeightNot_AboveBreakpoint() {
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(0, 2000, 840);
        assertEquals("Side-Sheet PCCT should be created", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_SideSheet_AboveBreakPoint() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Side-Sheet PCCT should be created", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_BottomSheet_HeightSetWidthNot() {
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(800, 0);
        assertEquals("Bottom-Sheet PCCT should be created", PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_BottomSheet_BelowBreakPoint() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Bottom-Sheet PCCT should be created", PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_BottomSheetStrategy() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Bottom-Sheet PCCT should be created", PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_SideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Side-Sheet PCCT should be created", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void transitionFromBottomSheetToSideSheetWhenOrientationChangedToLandscape() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals("Side-Sheet should be the active strategy", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET_FOR_THIRD_PARTIES})
    public void
    transitionFromBottomSheetTo900dpBottomSheetWhenOrientationChangedToLandscape_andDisable3P() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        // the density in this case is 1.0
        assertEquals("Should be 900dp width bottom sheet", BOTTOM_SHEET_MAX_WIDTH_DP,
                (int) (mPCCTTestRule.getWindowAttributes().width));
        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
    }

    @Test
    public void
    transitionFromBottomSheetTo900dpBottomSheetWhenOrientationChangedToLandscape_andHeightSetWidthNot() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(800, 0);

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        // the density in this case is 1.0
        assertEquals("Should be 900dp width bottom sheet", BOTTOM_SHEET_MAX_WIDTH_DP,
                (int) (mPCCTTestRule.getWindowAttributes().width));
        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
    }

    @Test
    public void transitionFromSideSheetToBottomSheetWhenOrientationChangedToPortrait() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Side-Sheet should be the active strategy", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
    }

    @Test
    public void dontTransitionIfOrientationDoesNotChange() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
    }

    @Test
    public void rotateInFullscreenMode() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        var attrs = mPCCTTestRule.getWindowAttributes();
        int height = attrs.height;
        int width = attrs.width;

        mFullscreen = true;
        displayManager.getSizeStrategyForTesting().onEnterFullscreen(null, null);

        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
        // Bottom-sheet strategy is now in action. Verify its top margin is removed
        // for the correct fullscreen UI.
        assertEquals("Top margin should be zero in fullscreen", 0,
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
    }

    @Test
    public void rotateInMaximizeMode() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        var attrs = mPCCTTestRule.getWindowAttributes();
        int height = attrs.height;
        int width = attrs.width;

        var sideSheetStrategy = displayManager.getSizeStrategyForTesting();
        ((PartialCustomTabSideSheetStrategy) sideSheetStrategy).toggleMaximize(true);

        assertTrue("Should be in maximized state.", sideSheetStrategy.isMaximized());

        mPCCTTestRule.configPortraitMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        // Strategy is now set to bottom sheet. Verify it starts in initial-height mode regardless
        // of the previous strategy state.
        var bottomSheetStrategy = displayManager.getSizeStrategyForTesting();
        assertFalse(
                "Bottom sheet must start in initial height.", bottomSheetStrategy.isMaximized());

        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        sideSheetStrategy = displayManager.getSizeStrategyForTesting();

        // When coming back to SideSheet strategy, the last state should be restored.
        assertTrue("Side sheet must start in minimized state.", sideSheetStrategy.isMaximized());

        ((PartialCustomTabSideSheetStrategy) sideSheetStrategy).toggleMaximize(true);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        attrs = mPCCTTestRule.getWindowAttributes();
        assertFalse("Should not be in maximized state.", sideSheetStrategy.isMaximized());
        assertEquals("Height should be restored.", height, attrs.height);
        assertEquals("Width should be restored.", width, attrs.width);
    }

    @Test
    public void calculatePartialCustomTabTypePermutations() {
        int initWidth = 0;
        int initHeight = 0;
        Supplier<Integer> displayWidthDp = null;
        int breakPointDp = 0;
        boolean ssEnabled = true; // side sheet feature flag

        // Multi-window mode -> FULL
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        assertEquals("Type should be FULL_SIZE", PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));

        // Zero initial width/height -> FULL
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
        assertEquals("Type should be FULL_SIZE", PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));

        // Non-zero height -> BOTTOM_SHEET
        initWidth = 0;
        initHeight = 500;
        assertEquals("Type should be BOTTOM_SHEET", PartialCustomTabType.BOTTOM_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));

        // Non-zero width -> either FULL_SIZE or SIDE_SHEET
        initWidth = 500;
        initHeight = 0;
        displayWidthDp = () -> 1000;
        breakPointDp = 1200;
        assertEquals("Type should be FULL_SIZE", PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));

        breakPointDp = 800;
        assertEquals("Type should be SIDE_SHEET", PartialCustomTabType.SIDE_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));

        ssEnabled = false;
        assertEquals("Type should be FULL_SIZE", PartialCustomTabType.FULL_SIZE,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));
        ssEnabled = true;

        // Non-zero width/height -> either SIDE_SHEET or BOTTOM_SHEET
        initWidth = 300;
        initHeight = 500;
        displayWidthDp = () -> 1000;
        breakPointDp = 400;
        assertEquals("Type should be SIDE_SHEET", PartialCustomTabType.SIDE_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));

        ssEnabled = false;
        assertEquals("Type should be BOTTOM SHEET", PartialCustomTabType.BOTTOM_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));
        ssEnabled = true;

        initWidth = 300;
        initHeight = 500;
        displayWidthDp = () -> 1000;
        breakPointDp = 1200;
        assertEquals("Type should be BOTTOM_SHEET", PartialCustomTabType.BOTTOM_SHEET,
                PartialCustomTabDisplayManager.calculatePartialCustomTabType(
                        null, initWidth, initHeight, displayWidthDp, breakPointDp, ssEnabled));
    }

    @Test
    public void startAnimationOverride() {
        int defId = 42;
        var provider = Mockito.mock(BrowserServicesIntentDataProvider.class);
        when(provider.getAnimationEnterRes()).thenReturn(defId);

        // FULL_SIZE -> slide up
        assertEquals(R.anim.slide_in_up,
                PartialCustomTabDisplayManager.getStartAnimationOverride(null, provider, defId));

        // BOTTOM_SHEET -> slide up
        when(provider.getInitialActivityHeight()).thenReturn(1000);
        assertEquals(R.anim.slide_in_up,
                PartialCustomTabDisplayManager.getStartAnimationOverride(null, provider, defId));

        // SIDE_SHEET -> slide from side
        when(provider.getSideSheetPosition()).thenReturn(ACTIVITY_SIDE_SHEET_POSITION_END);
        when(provider.getInitialActivityWidth()).thenReturn(1000);
        when(provider.getActivityBreakPoint()).thenReturn(500);

        Activity act = mPCCTTestRule.mActivity;
        when(provider.getSideSheetSlideInBehavior())
                .thenReturn(ACTIVITY_SIDE_SHEET_SLIDE_IN_FROM_SIDE);
        LocalizationUtils.setRtlForTesting(false);
        assertEquals(R.anim.slide_in_right,
                PartialCustomTabDisplayManager.getStartAnimationOverride(act, provider, defId));

        LocalizationUtils.setRtlForTesting(true);
        assertEquals(R.anim.slide_in_left,
                PartialCustomTabDisplayManager.getStartAnimationOverride(act, provider, defId));
    }
}
