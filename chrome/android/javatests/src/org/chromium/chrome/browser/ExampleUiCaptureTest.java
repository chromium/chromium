// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.ui.base.DeviceFormFactor;

/** Simple test to demonstrate use of ScreenShooter rule. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE) // Tab switcher button only exists on phones.
public class ExampleUiCaptureTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public ScreenShooter mScreenShooter = new ScreenShooter();

    /** Capture the New Tab Page and the tab switcher. */
    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    public void testCaptureNewTabPage() {
        mActivityTestRule.startFromLauncherAtNtp();
        mScreenShooter.shoot("NTP", ScreenShooter.TagsEnum.UiCatalogueExample);
    }
}
