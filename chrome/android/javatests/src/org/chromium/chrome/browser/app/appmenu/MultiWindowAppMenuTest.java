// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertNotEquals;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Public Transit tests for operations through the app menu in multi-window. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Batching not yet supported in multi-window")
// In phones, the New Window option in the app menu is only enabled when already in multi-window or
// multi-display mode with Chrome not running in an adjacent window.
@Restriction({DeviceFormFactor.TABLET, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class MultiWindowAppMenuTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @LargeTest
    public void testOpenNewWindow_fromWebPage() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        assertInDifferentWindows(pageInFirstWindow, pageInSecondWindow);
        TransitAsserts.assertFinalDestinations(pageInFirstWindow, pageInSecondWindow);
    }

    @Test
    @LargeTest
    public void testOpenNewWindow_fromIncognitoNtp() {
        IncognitoNewTabPageStation pageInFirstWindow =
                mCtaTestRule.startOnBlankPage().openNewIncognitoTabFast();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openAppMenu().openNewWindow();

        assertInDifferentWindows(pageInFirstWindow, pageInSecondWindow);
        TransitAsserts.assertFinalDestinations(pageInFirstWindow, pageInSecondWindow);
    }

    static void assertInDifferentWindows(Station<?> station1, Station<?> station2) {
        assertNotEquals(station1.getActivity(), station2.getActivity());
        assertNotEquals(station1.getActivity().getWindow(), station2.getActivity().getWindow());
    }
}
