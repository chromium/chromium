// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestinations;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_RETURN_TIME;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
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
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/419289558): Re-enable color surface feature
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE,
    OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS
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
        IncognitoNewTabPageStation newIncognitoTab = appMenu.openNewIncognitoTabOrWindow();

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            assertFinalDestinations(tabSwitcher, newIncognitoTab);
        } else {
            assertFinalDestination(newIncognitoTab);
        }
    }

    @Test
    @LargeTest
    // TODO(crbug.com/457847264): Test disabled for Incognito windowing.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
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
    // TODO(crbug.com/461916575): Test disabled for Incognito windowing, delete once fixed
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testTabGroupPane_newTabGroup() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        int firstTabId = firstPage.loadedTabElement.value().getId();
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
                        .openNewIncognitoTabOrWindowFast()
                        .openIncognitoTabSwitcher()
                        .openAppMenu()
                        .openNewTabGroup()
                        .pressDoneAsPartOfFlow()
                        .openNewIncognitoTab();

        // Go back to a PageStation for BlankCTATabInitialStateRule to reset state.
        // Reset not needed for incognito window.
        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            RegularNewTabPageStation secondPage =
                    incognitoNewTabPageStation.openAppMenu().openNewTab();
            assertFinalDestination(secondPage);
        } else {
            assertFinalDestinations(firstPage, incognitoNewTabPageStation);
        }
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

        newTabPage =
                mCtaTestRule
                        .pauseAndResumeActivityTo(tabSwitcher)
                        .arriveAt(
                                RegularNewTabPageStation.newBuilder()
                                        .initSelectingExistingTab()
                                        .build());

        assertFinalDestination(newTabPage);
    }
}
