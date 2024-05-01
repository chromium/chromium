// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.LargeTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.NewTabPageStation;
import org.chromium.chrome.test.transit.SettingsStation;

/** Public Transit tests for the app menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabbedAppMenuPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Test
    @LargeTest
    public void testOpenNewTab() {
        NewTabPageStation newTabPage =
                mInitialStateRule.startOnBlankPageBatched().openAppMenu().openNewTab();

        assertEquals(2, sActivityTestRule.tabsCount(/* incognito= */ false));
        assertEquals(0, sActivityTestRule.tabsCount(/* incognito= */ true));
        assertFinalDestination(newTabPage);
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        IncognitoNewTabPageStation newIncognitoTabPage =
                mInitialStateRule.startOnBlankPageBatched().openAppMenu().openNewIncognitoTab();

        assertEquals(1, sActivityTestRule.tabsCount(/* incognito= */ false));
        assertEquals(1, sActivityTestRule.tabsCount(/* incognito= */ true));
        assertFinalDestination(newIncognitoTabPage);
    }

    @Test
    @LargeTest
    public void testOpenSettings() {
        SettingsStation settings =
                mInitialStateRule.startOnBlankPageBatched().openAppMenu().openSettings();

        assertFinalDestination(settings);
    }
}
