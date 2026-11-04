// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;

/**
 * Example [instrumentation/on-device] [integration/app-wide], [batched], [activity-reused] test
 * that relies on tests to finish where they started.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ExampleReusedCtaTest {
    @Rule
    public ReusedCtaTransitTestRule<RegularNewTabPageStation> mActivityTestRule =
            ChromeTransitTestRules.ntpStartReusedActivityRule();

    @Test
    @LargeTest
    public void testOnNtp1() {
        RegularNewTabPageStation ntp = mActivityTestRule.start();
        ntp.openAppMenu().closeProgrammatically();
    }

    @Test
    @LargeTest
    public void testOnNtp2() {
        RegularNewTabPageStation ntp = mActivityTestRule.start();
        ntp.openRegularTabSwitcher().selectTabAtIndex(0, RegularNewTabPageStation.newBuilder());
    }
}
