// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.interpolators.Interpolators;

/** Unit tests for {@link HubPaneSwipeAnimationConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubPaneSwipeAnimationConfigUnitTest {
    @Test
    public void testGetSwipeSettleInterpolator_default() {
        assertEquals(
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR,
                HubPaneSwipeAnimationConfig.getSwipeSettleInterpolator());
    }

    @Test
    public void testGetSwipeSettleMaxDurationMs_default() {
        assertEquals(250L, HubPaneSwipeAnimationConfig.getSwipeSettleMaxDurationMs());
        assertEquals(160L, HubPaneSwipeAnimationConfig.getSwipeSettleMinDurationMs(250L));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE + ":use_emphasized_interpolator/true")
    public void testGetSwipeSettleInterpolator_emphasized() {
        assertEquals(
                Interpolators.EMPHASIZED, HubPaneSwipeAnimationConfig.getSwipeSettleInterpolator());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE + ":max_duration_ms/200")
    public void testGetSwipeSettleMaxDurationMs_custom200ms() {
        assertEquals(200L, HubPaneSwipeAnimationConfig.getSwipeSettleMaxDurationMs());
        assertEquals(128L, HubPaneSwipeAnimationConfig.getSwipeSettleMinDurationMs(200L));
    }

    @Test
    public void testGetSwipeSettleMinDurationMs_zeroOrNegative() {
        assertEquals(0L, HubPaneSwipeAnimationConfig.getSwipeSettleMinDurationMs(0L));
        assertEquals(0L, HubPaneSwipeAnimationConfig.getSwipeSettleMinDurationMs(-100L));
    }
}
