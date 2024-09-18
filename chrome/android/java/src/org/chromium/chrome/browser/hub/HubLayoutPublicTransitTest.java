// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB_SEARCH;
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
import org.chromium.base.test.util.Features.DisableFeatures;
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
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures(ANDROID_HUB_SEARCH)
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
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();

        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());

        assertFinalDestination(firstPage);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewTab() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();

        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();
        RegularNewTabPageStation newTab = appMenu.openNewTab();

        assertFinalDestination(newTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewIncognitoTab() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();

        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();
        IncognitoNewTabPageStation newIncognitoTab = appMenu.openNewIncognitoTab();

        assertFinalDestination(newIncognitoTab);
    }

    @Test
    @LargeTest
    public void testChangeTabSwitcherPanes() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        IncognitoNewTabPageStation incognitoNewTabPage =
                firstPage.openGenericAppMenu().openNewIncognitoTab();

        IncognitoTabSwitcherStation incognitoTabSwitcher =
                incognitoNewTabPage.openIncognitoTabSwitcher();
        assertEquals(
                incognitoTabSwitcher,
                incognitoTabSwitcher.selectPane(
                        PaneId.INCOGNITO_TAB_SWITCHER, IncognitoTabSwitcherStation.class));

        RegularTabSwitcherStation tabSwitcher =
                incognitoTabSwitcher.selectPane(
                        PaneId.TAB_SWITCHER, RegularTabSwitcherStation.class);

        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
        WebPageStation blankTab = tabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        assertFinalDestination(blankTab);
    }

    @Test
    @LargeTest
    @EnableFeatures({START_SURFACE_RETURN_TIME})
    public void testExitHubOnStartSurfaceAsNtp() {
        ReturnToChromeUtil.HOME_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);

        WebPageStation blankPage = mInitialStateRule.startOnBlankPage();
        RegularNewTabPageStation newTabPage = blankPage.openGenericAppMenu().openNewTab();
        RegularTabSwitcherStation tabSwitcher = newTabPage.openRegularTabSwitcher();
        blankPage = tabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        tabSwitcher = blankPage.openRegularTabSwitcher();

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
