// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Public transit tests for the Hub's tab switcher list editor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
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
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testCreateTabGroupOf1() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = firstPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);

        editor.openAppMenuWithEditor().groupTabs();

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testCreateTabGroupOf2() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
        int secondTabId = secondPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        editor.openAppMenuWithEditor().groupTabs();

        // Go back to PageStation for InitialStateRule to reset
        secondPage =
                tabSwitcher.leaveHubToPreviousTabViaBack(RegularNewTabPageStation.newBuilder());
        assertFinalDestination(secondPage);
    }

    @Test
    @MediumTest
    public void testClose2Tabs() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
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
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    @DisabledTest(message = "crbug.com/360800262")
    public void testCreateTabGroupOf2_parity() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        RegularNewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
        int secondTabId = secondPage.getLoadedTab().getId();
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        NewTabGroupDialogFacility dialog =
                editor.openAppMenuWithEditor().groupTabsWithParityEnabled();
        dialog = dialog.inputName("test_tab_group_name");
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        // Go back to PageStation for InitialStateRule to reset
        secondPage =
                tabSwitcher.leaveHubToPreviousTabViaBack(RegularNewTabPageStation.newBuilder());
        assertFinalDestination(secondPage);
    }
}
