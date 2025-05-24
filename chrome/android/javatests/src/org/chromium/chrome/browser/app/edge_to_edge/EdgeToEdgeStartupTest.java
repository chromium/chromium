// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.edge_to_edge;

import static org.junit.Assert.assertNull;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeBottomChinFacility;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Test looking to test startup behavior for edge to edge. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test interacts with startup and native initialization")
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION
})
@EnableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
    ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS
})
// Bots <= VERSION_CODES.S use 3-bottom nav bar. See crbug.com/352402600
@MinAndroidSdkLevel(VERSION_CODES.S_V2)
@Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class EdgeToEdgeStartupTest {

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setup() {
        EdgeToEdgeUtils.setObservedTappableNavigationBarForTesting(false);
    }

    @Test
    @LargeTest
    public void testStartOnNewPage() {
        mActivityTestRule
                .startOnBlankPage()
                .enterFacilitySync(new EdgeToEdgeBottomChinFacility<>(false), null);

        // Hop off, and assume invalid insets came in.
        EdgeToEdgeUtils.setObservedTappableNavigationBarForTesting(true);

        mActivityTestRule.recreateActivity();
        mActivityTestRule.waitForActivityNativeInitializationComplete();

        EdgeToEdgeController controller =
                mActivityTestRule.getActivity().getEdgeToEdgeControllerSupplierForTesting().get();
        assertNull(
                "Edge to edge should no longer recreate  after seeing tappable insets.",
                controller);
    }
}
