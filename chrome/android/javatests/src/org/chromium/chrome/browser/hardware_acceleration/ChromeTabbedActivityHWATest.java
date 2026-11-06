// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;

/** Tests that ChromeTabbedActivity is hardware accelerated only high-end devices. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeTabbedActivityHWATest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
    }

    @Test
    @SmallTest
    public void testHardwareAcceleration() throws Exception {
        Utils.assertHardwareAcceleration(mActivityTestRule.getActivity());
    }
}
