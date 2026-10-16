// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests starting the activity with URLs. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(b/555414915): Update Android tests with WebUI NTP enabled on AL.
@DisableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
public class MainActivityWithURLTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    /** Verify launch the activity with URL. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testLaunchActivityWithURL() {
        // Launch chrome
        String url =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/simple.html");
        mActivityTestRule
                .startOnUrlTo(url)
                .arriveAt(
                        WebPageStation.newBuilder()
                                .withEntryPoint()
                                .withExpectedUrlSubstring(url)
                                .withExpectedTitle("Activity test page")
                                .build());
    }

    /** Launch from the launcher and verify the NTP loads. */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP) // https://crbug.com/40247800
    public void testLaunchActivity() {
        // Launch chrome. Arriving at the NTP station verifies the NTP URL is loaded.
        mActivityTestRule.startFromLauncherAtNtp();
    }

    /**
     * Launch a NTP and make sure it loads correctly. This makes sure the NTP loading complete
     * notification is received.
     */
    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testNewTabPageLaunch() {
        // Launch chrome with NTP. Arriving at the NTP station verifies the NTP URL is loaded.
        RegularNewTabPageStation ntp = mActivityTestRule.startOnNtp();

        ntp.openAppMenu().openNewTab();
    }
}
