// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID;
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
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.HubIncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.HubTabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.HubTabSwitcherStation;
import org.chromium.chrome.test.transit.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.NewTabPageStation;
import org.chromium.chrome.test.transit.PageAppMenuFacility;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ANDROID_HUB})
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

        HubTabSwitcherStation tabSwitcher = page.openHub(HubTabSwitcherStation.class);

        PageStation previousTab = tabSwitcher.leaveHubToPreviousTabViaBack();

        assertFinalDestination(previousTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewTab() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        HubTabSwitcherStation tabSwitcher = page.openHub(HubTabSwitcherStation.class);

        HubTabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();

        NewTabPageStation newTab = appMenu.openNewTab();

        assertFinalDestination(newTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewIncognitoTab() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        HubTabSwitcherStation tabSwitcher = page.openHub(HubTabSwitcherStation.class);

        HubTabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();

        IncognitoNewTabPageStation newIncognitoTab = appMenu.openNewIncognitoTab();

        assertFinalDestination(newIncognitoTab);
    }

    @Test
    @LargeTest
    public void testChangeTabSwitcherPanes() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        PageAppMenuFacility appMenu = page.openGenericAppMenu();
        IncognitoNewTabPageStation incognitoNewTabPage = appMenu.openNewIncognitoTab();

        HubIncognitoTabSwitcherStation incognitoTabSwitcher =
                incognitoNewTabPage.openHub(HubIncognitoTabSwitcherStation.class);
        assertEquals(
                incognitoTabSwitcher,
                incognitoTabSwitcher.selectPane(
                        PaneId.INCOGNITO_TAB_SWITCHER, HubIncognitoTabSwitcherStation.class));

        HubTabSwitcherStation tabSwitcher =
                incognitoTabSwitcher.selectPane(PaneId.TAB_SWITCHER, HubTabSwitcherStation.class);

        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
        PageStation blankTab = tabSwitcher.selectTabAtIndex(0);
        assertFinalDestination(blankTab);
    }

    @Test
    @LargeTest
    @EnableFeatures({SHOW_NTP_AT_STARTUP_ANDROID, START_SURFACE_RETURN_TIME})
    public void testExitHubOnStartSurfaceAsNtp() {
        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS.setForTesting(0);

        PageStation page = mInitialStateRule.startOnBlankPage();

        PageAppMenuFacility appMenu = page.openGenericAppMenu();
        NewTabPageStation newTabPage = appMenu.openNewTab();

        HubTabSwitcherStation tabSwitcher = newTabPage.openHub(HubTabSwitcherStation.class);
        page = tabSwitcher.selectTabAtIndex(0);

        tabSwitcher = page.openHub(HubTabSwitcherStation.class);
        newTabPage = pauseAndResumeActivity(tabSwitcher);

        assertFinalDestination(newTabPage);
    }

    private NewTabPageStation pauseAndResumeActivity(Station currentStation) {
        NewTabPageStation destination =
                NewTabPageStation.newBuilder().withIsOpeningTabs(0).withIsSelectingTabs(1).build();
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
