// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.junit.Assert.assertEquals;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link PartialCustomTabFullSizeStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
public class PartialCustomTabFullSizeStrategyTest {
    private boolean mFullscreen;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabFullSizeStrategy createPcctFullSizeStrategy() {
        PartialCustomTabFullSizeStrategy pcct =
                new PartialCustomTabFullSizeStrategy(mPCCTTestRule.mActivity,
                        mPCCTTestRule.mOnResizedCallback, mPCCTTestRule.mFullscreenManager, false,
                        true, mPCCTTestRule.mHandleStrategyFactory);
        pcct.setMockViewForTesting(mPCCTTestRule.mCoordinatorLayout, mPCCTTestRule.mToolbarView,
                mPCCTTestRule.mToolbarCoordinator);
        return pcct;
    }

    @Test
    public void create_fullSizeStrategyInMultiWindowLandscape() {
        mPCCTTestRule.configLandscapeMode();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabFullSizeStrategy strategy = createPcctFullSizeStrategy();

        assertEquals("Full-Size PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.FULL_SIZE,
                strategy.getStrategyType());
        assertEquals("Full-Size has wrong height", MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
        assertEquals("Full-Size has wrong width", MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).width);
    }

    @Test
    public void create_fullSizeStrategyInMultiWindowPortrait() {
        mPCCTTestRule.configPortraitMode();
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        PartialCustomTabFullSizeStrategy strategy = createPcctFullSizeStrategy();

        assertEquals("Full-Size PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.FULL_SIZE,
                strategy.getStrategyType());
        assertEquals("Full-Size has wrong height", MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).height);
        assertEquals("Full-Size has wrong width", MATCH_PARENT,
                mPCCTTestRule.mAttributeResults.get(0).width);
    }
}