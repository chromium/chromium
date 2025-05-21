// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_RETURN_TIME;

import android.os.Build;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE
})
@Batch(Batch.PER_CLASS)
public class HubLayoutPublicTransitTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @LargeTest
    public void testEnterAndExitHub() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();

        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());

        assertFinalDestination(firstPage);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewTab() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();

        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();
        RegularNewTabPageStation newTab = appMenu.openNewTab();

        assertFinalDestination(newTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewIncognitoTab() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();

        TabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();
        IncognitoNewTabPageStation newIncognitoTab = appMenu.openNewIncognitoTab();

        assertFinalDestination(newIncognitoTab);
    }

    @Test
    @LargeTest
    public void testChangeTabSwitcherPanes() {
        IncognitoTabSwitcherStation incognitoTabSwitcher =
                mCtaTestRule
                        .startOnBlankPage()
                        .openNewIncognitoTabFast()
                        .openIncognitoTabSwitcher();

        RegularTabSwitcherStation regularTabSwitcher = incognitoTabSwitcher.selectRegularTabsPane();
        incognitoTabSwitcher = regularTabSwitcher.selectIncognitoTabsPane();

        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
        incognitoTabSwitcher.selectTabAtIndex(0, IncognitoNewTabPageStation.newBuilder());
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testTabGroupPane_newTabGroup() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        int firstTabId = firstPage.loadedTabElement.get().getId();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility<RegularTabSwitcherStation> editor =
                tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);

        NewTabGroupDialogFacility<RegularTabSwitcherStation> dialog =
                editor.openAppMenuWithEditor().groupTabs();
        dialog = dialog.inputName("test_tab_group_name");
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        RegularNewTabPageStation finalStation =
                tabSwitcher
                        .selectTabGroupsPane()
                        .createNewTabGroup()
                        .pressDoneAsPartOfFlow()
                        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
                        .openNewRegularTab();
        assertFinalDestination(finalStation);
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testRegularTabSwitcher_newTabGroup() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation finalPage =
                firstPage
                        .openRegularTabSwitcher()
                        .openAppMenu()
                        .openNewTabGroup()
                        .pressDoneAsPartOfFlow()
                        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
                        .openNewRegularTab();

        assertFinalDestination(finalPage);
    }

    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_ENTRY_POINTS_ANDROID)
    public void testIncognitoTabSwitcherStation_newTabGroup() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        IncognitoNewTabPageStation incognitoNewTabPageStation =
                firstPage
                        .openNewIncognitoTabFast()
                        .openIncognitoTabSwitcher()
                        .openAppMenu()
                        .openNewTabGroup()
                        .pressDoneAsPartOfFlow()
                        .openNewIncognitoTab();

        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
        RegularNewTabPageStation secondPage = incognitoNewTabPageStation.openAppMenu().openNewTab();
        assertFinalDestination(secondPage);
    }

    @Test
    @LargeTest
    @EnableFeatures({START_SURFACE_RETURN_TIME})
    public void testExitHubOnStartSurfaceAsNtp() {
        ChromeFeatureList.sStartSurfaceReturnTimeTabletSecs.setForTesting(0);

        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation newTabPage = blankPage.openNewTabFast();
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
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    ChromeApplicationTestUtils.fireHomeScreenIntent(cta);
                    try {
                        mCtaTestRule.resumeMainActivityFromLauncher();
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
