// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.RegularWebPageAppMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.List;

/** Public Transit tests for operations through the app menu in multi-window. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Batching not yet supported in multi-window")
// In phones, the New Window option in the app menu is only enabled when already in multi-window or
// multi-display mode with Chrome not running in an adjacent window.
@Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@EnableFeatures(ChromeFeatureList.TOOLBAR_TABLET_RESIZE_REFACTOR)
// TODO(b/555414915): Update Android tests with WebUI NTP enabled on AL.
@DisableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
public class MultiWindowAppMenuTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288091
    public void testOpenNewWindow_fromWebPage() {
        doTestOpenNewWindow();
    }

    private void doTestOpenNewWindow() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);
        TransitAsserts.assertFinalDestinations(pageInFirstWindow, pageInSecondWindow);
    }

    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // https://crbug.com/500749408
    public void testOpenNewWindow_fromIncognitoNtp() {
        doTestOpenNewWindow_fromIncognitoNtp();
    }

    private void doTestOpenNewWindow_fromIncognitoNtp() {
        WebPageStation blankPage = mCtaTestRule.startOnIncognitoBlankPage();
        IncognitoNewTabPageStation pageInFirstWindow = blankPage.openNewIncognitoTabFast();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);
        TransitAsserts.assertFinalDestinations(pageInFirstWindow, pageInSecondWindow);
    }

    @Test
    @LargeTest
    public void testOpenAndCloseNewWindow() {
        doTestOpenAndCloseNewWindow();
    }

    private void doTestOpenAndCloseNewWindow() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);

        pageInSecondWindow.finishActivity();

        TransitAsserts.assertFinalDestinations(pageInFirstWindow);
    }

    @Test
    @LargeTest
    public void testOpenNewWindowAndCloseOriginal() {
        doTestOpenNewWindowAndCloseOriginal();
    }

    private void doTestOpenNewWindowAndCloseOriginal() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        TransitAsserts.assertInDifferentTasks(pageInFirstWindow, pageInSecondWindow);

        pageInFirstWindow.finishActivity();

        TransitAsserts.assertFinalDestinations(pageInSecondWindow);
    }

    @Test
    @LargeTest
    @DisableFeatures({
        ChromeFeatureList.SETTINGS_MULTI_COLUMN,
        ChromeFeatureList.SETTINGS_IN_TAB, // crbug.com/521895796
        ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP // crbug.com/556881398
    })
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288091
    public void testInteractWithBothWindows() {
        doTestInteractWithBothWindows();
    }

    private void doTestInteractWithBothWindows() {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                pageInFirstWindow.openRegularTabAppMenu().openNewWindow();

        pageInFirstWindow.openRegularTabAppMenu().openSettings();
        pageInSecondWindow.openRegularTabSwitcher();
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_IN_APP_MENU,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS
    })
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM)
    public void testAddToGroup_showsGroupsFromOtherWindow() {
        WebPageStation page1InFirstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation pageInSecondWindow =
                page1InFirstWindow.openRegularTabAppMenu().openNewWindow();

        WebPageStation window2Page =
                Journeys.prepareTabs(
                        pageInSecondWindow, 2, 0, "about:blank", WebPageStation::newBuilder);
        RegularTabSwitcherStation tabSwitcher2 = window2Page.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCardWindow2 =
                Journeys.mergeAllTabsToNewGroup(tabSwitcher2);

        MultiWindowTestHelper.moveActivityToFront(page1InFirstWindow.getActivity());

        WebPageStation page2InFirstWindow =
                page1InFirstWindow.openRegularTabAppMenu().openNewTab().loadAboutBlank();

        RegularWebPageAppMenuFacility appMenu = page2InFirstWindow.openRegularTabAppMenu();
        appMenu.openTabGroupsSubmenu(
                /* expectedGroups= */ List.of(groupCardWindow2.getTitle()),
                /* excludedGroups= */ List.of());
        TransitAsserts.assertFinalDestinations(page2InFirstWindow, tabSwitcher2);
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.ONLY_TABLET)
    @EnableFeatures({
        ChromeFeatureList.SUBMENUS_IN_APP_MENU,
        ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS
    })
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM)
    public void testAddToGroup_closingGroupExcluded() {
        WebPageStation page1InFirstWindow = mCtaTestRule.startOnBlankPage();

        // 1. Prepare Window 1 tabs and create openGroup1 while Window 1 has 0 tab groups.
        WebPageStation page1Tabs =
                Journeys.prepareTabs(
                        page1InFirstWindow, 4, 0, "about:blank", WebPageStation::newBuilder);
        RegularTabSwitcherStation tabSwitcher1 = page1Tabs.openRegularTabSwitcher();
        TabModel tabModel1 = tabSwitcher1.tabModelElement.value();
        List<Tab> tabs1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> TabModelUtils.convertTabListToListOfTabs(tabModel1));
        TabSwitcherGroupCardFacility openGroup1 =
                Journeys.mergeTabsToNewGroup(
                        tabSwitcher1, List.of(tabs1.get(1), tabs1.get(2), tabs1.get(3)));

        WebPageStation page1 = tabSwitcher1.selectTabAtIndex(0, WebPageStation.newBuilder());

        // 2. Open Window 2 from Window 1.
        RegularNewTabPageStation pageInSecondWindow = page1.openRegularTabAppMenu().openNewWindow();

        // 3. Prepare Window 2 tabs and create closingGroup2 while Window 2 has 0 tab groups.
        WebPageStation window2Tabs =
                Journeys.prepareTabs(
                        pageInSecondWindow, 3, 0, "about:blank", WebPageStation::newBuilder);
        RegularTabSwitcherStation tabSwitcher2 = window2Tabs.openRegularTabSwitcher();
        TabModel tabModel2 = tabSwitcher2.tabModelElement.value();
        List<Tab> tabs2 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> TabModelUtils.convertTabListToListOfTabs(tabModel2));
        List<Tab> tabsToGroup2 = List.of(tabs2.get(1), tabs2.get(2));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel2.mergeListOfTabsToGroup(
                            tabsToGroup2,
                            tabsToGroup2.get(0),
                            TabGroupMergeNotificationType.DONT_NOTIFY);
                    tabModel2
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTabs(tabsToGroup2)
                                            .allowUndo(true)
                                            .build(),
                                    /* allowDialog= */ false);
                });

        // 4. Bring Window 1 back to the foreground and wait for ActivityState.RESUMED.
        MultiWindowTestHelper.moveActivityToFront(page1.getActivity());

        // 5. Open Window 1 app menu and verify tab groups submenu.
        RegularWebPageAppMenuFacility appMenu = page1.openRegularTabAppMenu();
        appMenu.openTabGroupsSubmenu(
                /* expectedGroups= */ List.of(openGroup1.getTitle()),
                /* excludedGroups= */ List.of("2 tabs"));

        TransitAsserts.assertFinalDestinations(page1, tabSwitcher2);
    }
}
