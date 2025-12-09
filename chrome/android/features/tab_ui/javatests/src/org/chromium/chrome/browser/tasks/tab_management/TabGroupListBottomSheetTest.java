// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestinations;

import android.util.Pair;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/**
 * Tests for the {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} classes for {@link
 * TabListCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE
})
@Batch(Batch.PER_CLASS)
public class TabGroupListBottomSheetTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testNewGroup_RegularNewTabPageStation() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabs(firstPage, 2, 0, "about:blank", WebPageStation::newBuilder);

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        RegularNewTabPageStation regularNtpStation =
                groupCard.clickCard().openNewRegularTab().openAppMenu().openNewTab();

        assertTabGroupsExist(regularNtpStation);
        assertCurrentTabIsNotInGroup(regularNtpStation);

        regularNtpStation
                .openAppMenu()
                .selectAddToGroupWithBottomSheet()
                .clickNewTabGroupRow()
                .pressDoneToExit();

        assertFinalDestination(regularNtpStation);
    }

    @Test
    @MediumTest
    public void testNewGroup_RegularWebPageStation() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabs(firstPage, 2, 0, "about:blank", WebPageStation::newBuilder);

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        RegularNewTabPageStation regularNtpStation =
                groupCard.clickCard().openNewRegularTab().openAppMenu().openNewTab();
        WebPageStation webPageStation = regularNtpStation.loadAboutBlank();

        assertTabGroupsExist(webPageStation);
        assertCurrentTabIsNotInGroup(webPageStation);

        webPageStation
                .openRegularTabAppMenu()
                .selectAddToGroupWithBottomSheet()
                .clickNewTabGroupRow()
                .pressDoneToExit();

        assertFinalDestination(webPageStation);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewGroup_IncognitoNewTabPageStation_Phone() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabs(firstPage, 1, 2, "about:blank", WebPageStation::newBuilder);

        IncognitoTabSwitcherStation tabSwitcher = pageStation.openIncognitoTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        IncognitoNewTabPageStation ntpStation =
                groupCard.clickCard().openNewIncognitoTab().openAppMenu().openNewIncognitoTab();

        assertTabGroupsExist(ntpStation);
        assertCurrentTabIsNotInGroup(ntpStation);

        ntpStation
                .openAppMenu()
                .selectAddToGroupWithBottomSheet()
                .clickNewTabGroupRow()
                .pressDoneToExit();
        RegularNewTabPageStation finalStation = ntpStation.openAppMenu().openNewTab();

        assertFinalDestination(finalStation);
    }

    @Test
    @MediumTest
    @ImportantFormFactors(DeviceFormFactor.TABLET_OR_DESKTOP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewGroup_IncognitoNewTabPageStation_Tablet() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        Pair<WebPageStation, WebPageStation> pageStations =
                Journeys.prepareTabsSeparateWindows(
                        firstPage, 1, 2, "about:blank", WebPageStation::newBuilder);

        IncognitoTabSwitcherStation tabSwitcher = pageStations.second.openIncognitoTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        IncognitoNewTabPageStation ntpStation =
                groupCard.clickCard().openNewIncognitoTab().openAppMenu().openNewIncognitoTab();

        assertTabGroupsExist(ntpStation);
        assertCurrentTabIsNotInGroup(ntpStation);

        ntpStation
                .openAppMenu()
                .selectAddToGroupWithBottomSheet()
                .clickNewTabGroupRow()
                .pressDoneToExit();
        RegularNewTabPageStation finalStation = ntpStation.openAppMenu().openNewTab();

        assertFinalDestinations(pageStations.first, ntpStation, finalStation);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewGroup_IncognitoWebPageStation_Phone() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabs(firstPage, 1, 2, "about:blank", WebPageStation::newBuilder);

        IncognitoTabSwitcherStation tabSwitcher = pageStation.openIncognitoTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        WebPageStation webPageStation =
                groupCard
                        .clickCard()
                        .openNewIncognitoTab()
                        .openAppMenu()
                        .openNewIncognitoTab()
                        .loadAboutBlank();

        assertTabGroupsExist(webPageStation);
        assertCurrentTabIsNotInGroup(webPageStation);

        webPageStation
                .openIncognitoTabAppMenu()
                .selectAddToGroupWithBottomSheet()
                .clickNewTabGroupRow()
                .pressDoneToExit();
        RegularNewTabPageStation finalStation =
                webPageStation.openIncognitoTabAppMenu().openNewTab();

        assertFinalDestination(finalStation);
    }

    @Test
    @MediumTest
    @ImportantFormFactors(DeviceFormFactor.TABLET_OR_DESKTOP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testNewGroup_IncognitoWebPageStation_Tablet() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        Pair<WebPageStation, WebPageStation> pageStations =
                Journeys.prepareTabsSeparateWindows(
                        firstPage, 1, 2, "about:blank", WebPageStation::newBuilder);

        IncognitoTabSwitcherStation tabSwitcher = pageStations.second.openIncognitoTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        WebPageStation webPageStation =
                groupCard
                        .clickCard()
                        .openNewIncognitoTab()
                        .openAppMenu()
                        .openNewIncognitoTab()
                        .loadAboutBlank();

        assertTabGroupsExist(webPageStation);
        assertCurrentTabIsNotInGroup(webPageStation);

        webPageStation
                .openIncognitoTabAppMenu()
                .selectAddToGroupWithBottomSheet()
                .clickNewTabGroupRow()
                .pressDoneToExit();
        RegularNewTabPageStation finalStation =
                webPageStation.openIncognitoTabAppMenu().openNewTab();

        assertFinalDestinations(pageStations.first, webPageStation, finalStation);
    }

    private static void assertTabGroupsExist(CtaPageStation pageStation) {
        int tabGroupCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> pageStation.getTabGroupModelFilter().getTabGroupCount());
        assertTrue(tabGroupCount > 0);
    }

    private static void assertCurrentTabIsNotInGroup(CtaPageStation pageStation) {
        Tab currentTab = pageStation.getTab();
        assertNull(currentTab.getTabGroupId());
    }
}
