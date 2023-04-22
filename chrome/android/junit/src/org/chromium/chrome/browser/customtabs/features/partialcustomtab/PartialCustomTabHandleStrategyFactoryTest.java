// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.PartialCustomTabType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;

/** Tests for {@link PartialCustomTabHandleStrategyFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Features.EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_SIDE_SHEET})
public class PartialCustomTabHandleStrategyFactoryTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final PartialCustomTabTestRule mPCCTTestRule = new PartialCustomTabTestRule();

    @Test
    public void create_PartialCustomTabHandleStrategyForSideSheet() {
        PartialCustomTabHandleStrategyFactory factory = new PartialCustomTabHandleStrategyFactory();
        var handleStrategy =
                factory.create(PartialCustomTabType.SIDE_SHEET, null, null, null, null, null);

        assertTrue("The handle strategy for side-sheet should be SimpleHandleStrategy",
                handleStrategy instanceof SimpleHandleStrategy);
    }

    @Test
    public void create_PartialCustomTabHandleStrategyForBottomSheet() {
        PartialCustomTabHandleStrategyFactory factory = new PartialCustomTabHandleStrategyFactory();
        var handleStrategy =
                factory.create(PartialCustomTabType.BOTTOM_SHEET, null, null, null, null, null);

        assertNotNull("The handle strategy for bottom-sheet should not be null", handleStrategy);
    }

    @Test
    public void create_PartialCustomTabHandleStrategyForFullSize() {
        PartialCustomTabHandleStrategyFactory factory = new PartialCustomTabHandleStrategyFactory();
        var handleStrategy =
                factory.create(PartialCustomTabType.FULL_SIZE, null, null, null, null, null);

        assertTrue("The handle strategy for full-size should be SimpleHandleStrategy",
                handleStrategy instanceof SimpleHandleStrategy);
    }
}
