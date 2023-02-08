// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.content.res.Configuration;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager.SizeStrategyCreator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link PartialCustomTabDisplayManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
public class PartialCustomTabDisplayManagerTest {
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
                mPCCTTestRule.mFullscreenManager, false, true);
        var sizeStrategyCreator = displayManager.getSizeStrategyCreatorForTesting();
        SizeStrategyCreator testSizeStrategyCreator = (type) -> {
            var strategy = sizeStrategyCreator.createForType(type);
            strategy.setFullscreenSupplierForTesting(() -> mFullscreen);
            return strategy;
        };
        displayManager.setMocksForTesting(mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mCustomTabToolbar, mPCCTTestRule.mHandleStrategyFactory,
                testSizeStrategyCreator);
        return displayManager;
    }

    @Test
    public void create_FullSize_NoFlagsSet() {
        // TODO(crbug.com/1411917) To be implemented once prerequisite work is completed
    }

    @Test
    public void create_SideSheet_WidthSetHeightNot_BelowBreakpoint() {
        // TODO(crbug.com/1411917) To be implemented once prerequisite work is completed
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
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mPCCTTestRule.configLandscapeMode();
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);

        PartialCustomTabTestRule.waitForAnimationToFinish();

        assertEquals("Side-Sheet should be the active strategy", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void transitionFromSideSheetToBottomSheetWhenOrientationChangedToPortrait() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        assertEquals("Side-Sheet should be the active strategy", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
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
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
    }

    @Test
    public void rotateInFullscreenMode() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager();

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        var attrs = mPCCTTestRule.getWindowAttributes();
        int height = attrs.height;
        int width = attrs.width;

        mFullscreen = true;
        displayManager.getSizeStrategyForTesting().onEnterFullscreen(null, null);

        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        // Bottom-sheet strategy is now in action. Verify its top margin is removed
        // for the correct fullscreen UI.
        assertEquals("Top margin should be zero in fullscreen", 0,
                displayManager.getSizeStrategyForTesting().getTopMarginForTesting());

        mFullscreen = false;
        displayManager.getSizeStrategyForTesting().onExitFullscreen(null);
        PartialCustomTabTestRule.waitForAnimationToFinish();

        attrs = mPCCTTestRule.getWindowAttributes();
        assertFalse("Should not be in fulllscreen.", attrs.isFullscreen());
        assertEquals("Height should be restored.", height, attrs.height);
        assertEquals("Width should be restored.", width, attrs.width);
    }
}
