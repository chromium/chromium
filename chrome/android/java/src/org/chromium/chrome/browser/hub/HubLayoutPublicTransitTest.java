// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_RETURN_TIME;

import android.os.Build;

import androidx.test.filters.LargeTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.PageAppMenuFacility;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class HubLayoutPublicTransitTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Test
    @LargeTest
    public void testEnterAndExitHub() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        RegularTabSwitcherStation tabSwitcher = page.openHub(RegularTabSwitcherStation.class);

        PageStation previousTab = tabSwitcher.leaveHubToPreviousTabViaBack();

        assertFinalDestination(previousTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewTab() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        RegularTabSwitcherStation tabSwitcher = page.openHub(RegularTabSwitcherStation.class);

        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();

        RegularNewTabPageStation newTab = appMenu.openNewTab();

        assertFinalDestination(newTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewIncognitoTab() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        RegularTabSwitcherStation tabSwitcher = page.openHub(RegularTabSwitcherStation.class);

        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();

        IncognitoNewTabPageStation newIncognitoTab = appMenu.openNewIncognitoTab();

        assertFinalDestination(newIncognitoTab);
    }

    @Test
    @LargeTest
    public void testChangeTabSwitcherPanes() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        PageAppMenuFacility appMenu = page.openGenericAppMenu();
        IncognitoNewTabPageStation incognitoNewTabPage = appMenu.openNewIncognitoTab();

        IncognitoTabSwitcherStation incognitoTabSwitcher =
                incognitoNewTabPage.openHub(IncognitoTabSwitcherStation.class);
        assertEquals(
                incognitoTabSwitcher,
                incognitoTabSwitcher.selectPane(
                        PaneId.INCOGNITO_TAB_SWITCHER, IncognitoTabSwitcherStation.class));

        RegularTabSwitcherStation tabSwitcher =
                incognitoTabSwitcher.selectPane(
                        PaneId.TAB_SWITCHER, RegularTabSwitcherStation.class);

        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
        PageStation blankTab = tabSwitcher.selectTabAtIndex(0);
        assertFinalDestination(blankTab);
    }

    @Test
    @LargeTest
    @EnableFeatures({START_SURFACE_RETURN_TIME})
    public void testExitHubOnStartSurfaceAsNtp() {
        ReturnToChromeUtil.HOME_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);

        PageStation page = mInitialStateRule.startOnBlankPage();

        PageAppMenuFacility appMenu = page.openGenericAppMenu();
        RegularNewTabPageStation newTabPage = appMenu.openNewTab();

        RegularTabSwitcherStation tabSwitcher = newTabPage.openHub(RegularTabSwitcherStation.class);
        page = tabSwitcher.selectTabAtIndex(0);

        tabSwitcher = page.openHub(RegularTabSwitcherStation.class);
        newTabPage = pauseAndResumeActivity(tabSwitcher);

        assertFinalDestination(newTabPage);
    }

    private RegularNewTabPageStation pauseAndResumeActivity(Station currentStation) {
        RegularNewTabPageStation destination =
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build();
        currentStation.travelToSync(
                destination,
                () -> {
                    ChromeTabbedActivity cta = sActivityTestRule.getActivity();
                    ChromeApplicationTestUtils.fireHomeScreenIntent(cta);
                    try {
                        sActivityTestRule.resumeMainActivityFromLauncher();
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });

        // crbug.com/324106495: Add an extra sleep in Android 12+ because SnapshotStartingWindow
        // occludes the ChromeActivity and any input is considered an untrusted input until the
        // SnapshotStartingWindow disappears.
        // Since it is a system window being drawn on top, we don't have access to any signals that
        // the SnapshotStartingWindow disappeared that we can wait for.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
            }
        }

        return destination;
    }
}
