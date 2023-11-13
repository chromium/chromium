// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_REFACTOR;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BasePageStation;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.HubStation;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ANDROID_HUB, START_SURFACE_REFACTOR})
// TODO(crbug/1498446): Add support for batching.
@DoNotBatch(reason = "Public transit lacks batch support. Hub does not exit cleanly yet.")
public class HubLayoutPublicTransitTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mActivityTestRule);

    @Test
    @LargeTest
    public void testEnterHub() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPage();

        HubStation hubStation = page.openHub();

        assertFinalDestination(hubStation);
    }

    @Test
    @LargeTest
    public void testEnterAndExitHub() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPage();

        HubStation hubStation = page.openHub();

        PageStation previousTab = hubStation.leaveHubToPreviousTabViaBack();

        assertFinalDestination(previousTab);
    }
}
