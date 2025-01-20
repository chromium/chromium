// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.test.util.TabBinningUtil.group;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherTabCardFacility;
import org.chromium.chrome.test.transit.hub.UndoGroupSnackbarFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.TabBinningUtil;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

/** Public transit tests for the Hub's tab switcher list editor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
public class TabSwitcherListEditorPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Test
    @MediumTest
    public void testLeaveEditorViaBackPress() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor.pressBackToExit();

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }

    @Test
    @MediumTest
    public void testCreateTabGroupOf1() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);

        NewTabGroupDialogFacility dialog = editor.openAppMenuWithEditor()
                .groupTabs();
        dialog = dialog.inputName("test_tab_group_name");
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }

    @Test
    @MediumTest
    public void testClose2Tabs() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openNewTabFast();
        int secondTabId = secondPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        editor.openAppMenuWithEditor().closeTabs();

        // Go back to PageStation for InitialStateRule to reset

        // Dismiss the undo snackbar because it might overlap with the New Tab button.
        ThreadUtils.runOnUiThreadBlocking(
                () -> tabSwitcher.getActivity().getSnackbarManager().dismissAllSnackbars());

        RegularNewTabPageStation ntp = tabSwitcher.openNewTab();
        assertFinalDestination(ntp);
    }

    @Test
    @MediumTest
    public void testCreateTabGroupOf2() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openNewTabFast();
        int secondTabId = secondPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        NewTabGroupDialogFacility dialog =
                editor.openAppMenuWithEditor().groupTabs();
        dialog = dialog.inputName("test_tab_group_name");
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        // Go back to PageStation for InitialStateRule to reset
        secondPage =
                tabSwitcher.leaveHubToPreviousTabViaBack(RegularNewTabPageStation.newBuilder());
        assertFinalDestination(secondPage);
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/378502216")
    public void testCreateTabGroupOf10() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        WebPageStation pageStation =
            Journeys.prepareTabsWithThumbnails(
                firstPage,
                10,
                0,
                "about:blank",
                WebPageStation::newBuilder
            );
        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/378502216")
    public void testCreate10TabsAndCreateTabGroupOf4() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        WebPageStation pageStation =
            Journeys.prepareTabsWithThumbnails(
                firstPage,
                10,
                0,
                "about:blank",
                WebPageStation::newBuilder
            );
        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabList tabList = tabSwitcher.getTabModelSelectorSupplier().get()
            .getCurrentModel().getComprehensiveModel();
        List<Tab> tabs = List.of(tabList.getTabAt(0),
                tabList.getTabAt(3), tabList.getTabAt(5), tabList.getTabAt(9));
        Journeys.mergeTabsToNewGroup(tabSwitcher, tabs);

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/378502216")
    public void testCreate2TabGroups() {
        WebPageStation pageStation = mInitialStateRule.startOnBlankPage();
        pageStation =
                Journeys.prepareTabsWithThumbnails(
                        pageStation, 10, 0, "about:blank", WebPageStation::newBuilder);

        TabModel currentModel = pageStation.getActivity().getCurrentTabModel();
        List<Tab> tabGroup1 = List.of(currentModel.getTabAt(0), currentModel.getTabAt(3));
        List<Tab> tabGroup2 =
                List.of(
                        currentModel.getTabAt(1),
                        currentModel.getTabAt(7),
                        currentModel.getTabAt(8));

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard =
                Journeys.mergeTabsToNewGroup(tabSwitcher, tabGroup1);
        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        tabGroupDialogFacility.pressBackArrowToExit();

        groupCard = Journeys.mergeTabsToNewGroup(tabSwitcher, tabGroup2);
        tabGroupDialogFacility = groupCard.clickCard();
        tabGroupDialogFacility.pressBackArrowToExit();

        // Go back to PageStation for InitialStateRule to reset
        pageStation = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(pageStation);
    }

    @Test
    @MediumTest
    public void testUndoCreateTabGroup() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();

        TabModel tabModel = firstPage.getActivity().getCurrentTabModel();

        // Open 3 tabs
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openNewTabFast();
        int secondTabId = secondPage.getLoadedTab().getId();
        RegularNewTabPageStation thirdPage = secondPage.openNewTabFast();
        int thirdTabId = thirdPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = thirdPage.openRegularTabSwitcher();

        // Group first and second tabs
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);
        NewTabGroupDialogFacility dialog = editor.openAppMenuWithEditor().groupTabs();
        dialog.pressDone();
        TabBinningUtil.assertBinsEqual(tabModel, group(secondTabId, firstTabId), thirdTabId);

        // Group all tabs; needed to bypass the New Tab Group dialog
        editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabGroupToSelection(0, List.of(firstTabId, secondTabId));
        editor = editor.addTabToSelection(1, thirdTabId);
        var groupMergedResult = editor.openAppMenuWithEditor().groupTabsWithoutDialog();
        TabBinningUtil.assertBinsEqual(tabModel, group(secondTabId, firstTabId, thirdTabId));

        // Ungroup and revert to only first and second tabs grouped, and third tab by itself
        UndoGroupSnackbarFacility undoSnackbar = groupMergedResult.second;
        undoSnackbar.pressUndo();
        tabSwitcher.expectGroupCard(
                List.of(firstTabId, secondTabId),
                TabSwitcherGroupCardFacility.DEFAULT_N_TABS_TITLE);
        TabSwitcherTabCardFacility thirdTabCard = tabSwitcher.expectTabCard(thirdTabId, "New tab");
        TabBinningUtil.assertBinsEqual(tabModel, group(secondTabId, firstTabId), thirdTabId);

        // Go back to PageStation for InitialStateRule to reset
        thirdPage = thirdTabCard.clickCard(RegularNewTabPageStation.newBuilder());
        assertFinalDestination(thirdPage);
    }
}
