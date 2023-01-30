// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertEquals;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link PartialCustomTabDisplayManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
public class PartialCustomTabDisplayManagerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabDisplayManager createPcctDisplayManager(@Px int heightPx) {
        PartialCustomTabDisplayManager displayManager = new PartialCustomTabDisplayManager(
                mPCCTTestRule.mActivity, heightPx, false, mPCCTTestRule.mOnResizedCallback,
                mPCCTTestRule.mActivityLifecycleDispatcher, mPCCTTestRule.mFullscreenManager, false,
                true);
        displayManager.setMockViewForTesting(mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mCustomTabToolbar, mPCCTTestRule.mHandleStrategyFactory);
        return displayManager;
    }

    @Test
    public void create_BottomSheetStrategy() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(2000);

        assertEquals("Bottom-Sheet PCCT should be created", PartialCustomTabType.BOTTOM_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void create_SideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(2000);

        assertEquals("Side-Sheet PCCT should be created", PartialCustomTabType.SIDE_SHEET,
                displayManager.getActiveStrategyType());
    }

    @Test
    public void transitionFromBottomSheetToSideSheetWhenOrientationChangedToLandscape() {
        mPCCTTestRule.configPortraitMode();
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(2000);

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
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(2000);

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
        PartialCustomTabDisplayManager displayManager = createPcctDisplayManager(2000);

        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
        mPCCTTestRule.mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        displayManager.onConfigurationChanged(mPCCTTestRule.mConfiguration);
        assertEquals("Bottom-Sheet should be the active strategy",
                PartialCustomTabType.BOTTOM_SHEET, displayManager.getActiveStrategyType());
    }
}
