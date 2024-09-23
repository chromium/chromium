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

import org.chromium.base.test.transit.ScrollableFacility;
import org.chromium.base.test.transit.ScrollableFacility.Item.Presence;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.IncognitoWebPageAppMenuFacility;
import org.chromium.chrome.test.transit.page.RegularWebPageAppMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;

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

    /** Tests that "New tab" opens a new tab with the NTP. */
    @Test
    @LargeTest
    public void testOpenNewTab() {
        RegularNewTabPageStation newTabPage =
                mInitialStateRule.startOnBlankPage().openRegularTabAppMenu().openNewTab();

        assertEquals(2, sActivityTestRule.tabsCount(/* incognito= */ false));
        assertEquals(0, sActivityTestRule.tabsCount(/* incognito= */ true));
        assertFinalDestination(newTabPage);
    }

    /** Tests that "New Incognito tab" opens a new incognito tab with the incognito NTP. */
    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        IncognitoNewTabPageStation newIncognitoTabPage =
                mInitialStateRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();

        assertEquals(1, sActivityTestRule.tabsCount(/* incognito= */ false));
        assertEquals(1, sActivityTestRule.tabsCount(/* incognito= */ true));
        assertFinalDestination(newIncognitoTabPage);
    }

    /** Tests that "Settings" opens the SettingsActivity. */
    @Test
    @LargeTest
    public void testOpenSettings() {
        WebPageStation pageStation = mInitialStateRule.startOnBlankPage();
        Tab tab = pageStation.getLoadedTab();
        SettingsStation settings = pageStation.openRegularTabAppMenu().openSettings();

        assertFinalDestination(settings);

        // Exit settings for the initial state rule to be able to reset state.
        settings.pressBack(
                WebPageStation.newBuilder()
                        .withIncognito(false)
                        .withIsOpeningTabs(0)
                        .withTabAlreadySelected(tab)
                        .build());
    }

    /**
     * Tests that all expected items declared in NewTabPageRegularAppMenuFacility are present in the
     * app menu opened from a regular NTP.
     */
    @Test
    @LargeTest
    public void testNewTabPageRegularAppMenuItems() {
        WebPageStation blankPage = mInitialStateRule.startOnBlankPage();
        RegularNewTabPageStation newTabPage = blankPage.openRegularTabAppMenu().openNewTab();
        RegularNewTabPageAppMenuFacility menu = newTabPage.openAppMenu();

        verifyPresentItems(menu);
        assertFinalDestination(newTabPage, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Tests that all expected items declared in NewTabPageIncognitoAppMenuFacility are present in
     * the app menu opened from an incognito NTP.
     */
    @Test
    @LargeTest
    public void testNewTabPageIncognitoAppMenuItems() {
        IncognitoNewTabPageStation incognitoNewTabPage =
                mInitialStateRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();
        IncognitoNewTabPageAppMenuFacility menu = incognitoNewTabPage.openAppMenu();

        verifyPresentItems(menu);
        assertFinalDestination(incognitoNewTabPage, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Tests that all expected items declared in WebPageRegularAppMenuFacility are present in the
     * app menu opened from a regular Tab displaying a web page.
     */
    @Test
    @LargeTest
    @RequiresRestart
    public void testWebPageRegularAppMenuItems() {
        WebPageStation blankPage = mInitialStateRule.startOnBlankPage();
        RegularWebPageAppMenuFacility menu = blankPage.openRegularTabAppMenu();

        verifyPresentItems(menu);
        assertFinalDestination(blankPage, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Tests that all expected items declared in WebPageIncognitoAppMenuFacility are present in the
     * app menu opened from an incognito Tab displaying a web page.
     */
    @Test
    @LargeTest
    public void testWebPageIncognitoAppMenuItems() {
        IncognitoNewTabPageStation incognitoNtp =
                mInitialStateRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();

        WebPageStation pageOne =
                incognitoNtp.loadPageProgrammatically(
                        sActivityTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                        NavigatePageStations.newNavigateOnePageBuilder());
        IncognitoWebPageAppMenuFacility menu = pageOne.openIncognitoTabAppMenu();

        verifyPresentItems(menu);
        assertFinalDestination(pageOne, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Scroll to each declared menu item and check they are there with the expected enabled state.
     */
    private static <T extends ScrollableFacility<?>> void verifyPresentItems(T menu) {
        for (ScrollableFacility<?>.Item<?> item : menu.getItems()) {
            if (item.getPresence() == Presence.PRESENT_AND_ENABLED
                    || item.getPresence() == Presence.PRESENT_AND_DISABLED) {
                item.scrollTo();
            }
        }
    }
}
