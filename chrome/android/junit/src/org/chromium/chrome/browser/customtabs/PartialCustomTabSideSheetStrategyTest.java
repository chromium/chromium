// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.customtabs.PartialCustomTabTestRule.DEVICE_HEIGHT_LANDSCAPE;
import static org.chromium.chrome.browser.customtabs.PartialCustomTabTestRule.DEVICE_WIDTH_LANDSCAPE;

import androidx.annotation.Px;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link PartialCustomTabSideSheetStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class PartialCustomTabSideSheetStrategyTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    private PartialCustomTabSideSheetStrategy createPcctSideSheetStrategy(@Px int heightPx) {
        PartialCustomTabSideSheetStrategy pcct = new PartialCustomTabSideSheetStrategy(
                mPCCTTestRule.mActivity, heightPx, false, mPCCTTestRule.mOnResizedCallback,
                mPCCTTestRule.mFullscreenManager, false, true);
        pcct.setMockViewForTesting();
        return pcct;
    }

    @Test
    public void create_sideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals("Side-Sheet PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                strategy.getStrategyType());
    }

    @Test
    public void create_largeHeightLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(5000);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void create_smallHeightLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(100);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void create_deviceHeightLandscape() {
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_HEIGHT_LANDSCAPE);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertTabIsAtFullLandscapeHeight();
    }

    @Test
    public void create_widthHalfWindowLandscape() {
        // TODO(crbug.com/1406104): This test case will be redone once we support configurable width
        mPCCTTestRule.configLandscapeMode();
        createPcctSideSheetStrategy(DEVICE_HEIGHT_LANDSCAPE);
        mPCCTTestRule.verifyWindowFlagsSet();

        assertEquals(
                "Should only have one attribute result", 1, mPCCTTestRule.mAttributeResults.size());
        assertEquals(DEVICE_WIDTH_LANDSCAPE / 2, mPCCTTestRule.mAttributeResults.get(0).width);
    }

    private void assertTabIsAtFullLandscapeHeight() {
        assertEquals(
                "Should only have one attribute result", 1, mPCCTTestRule.mAttributeResults.size());
        assertEquals(DEVICE_HEIGHT_LANDSCAPE, mPCCTTestRule.mAttributeResults.get(0).height);
    }
}
