// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;

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
        return new PartialCustomTabSideSheetStrategy(mPCCTTestRule.mActivity, heightPx, false,
                mPCCTTestRule.mOnResizedCallback, mPCCTTestRule.mFullscreenManager, false, true);
    }

    @Test
    public void create_SideSheetStrategy() {
        mPCCTTestRule.configLandscapeMode();
        PartialCustomTabSideSheetStrategy strategy = createPcctSideSheetStrategy(2000);

        assertEquals("Side-Sheet PCCT should be created",
                PartialCustomTabBaseStrategy.PartialCustomTabType.SIDE_SHEET,
                strategy.getStrategyType());
    }
}
