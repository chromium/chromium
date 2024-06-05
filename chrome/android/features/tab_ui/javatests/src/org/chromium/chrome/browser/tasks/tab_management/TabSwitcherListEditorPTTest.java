// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.HubTabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.HubTabSwitcherStation;
import org.chromium.chrome.test.transit.NewTabPageStation;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.WebPageStation;
import org.chromium.chrome.test.transit.hub.HubNewTabGroupDialogFacility;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Public transit tests for the Hub's tab switcher list editor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ANDROID_HUB})
@Batch(Batch.PER_CLASS)
public class TabSwitcherListEditorPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Test
    @MediumTest
    public void testCreateTabGroupOf1() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        HubTabSwitcherStation tabSwitcher = firstPage.openHub(HubTabSwitcherStation.class);
        HubTabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);

        editor.openAppMenuWithEditor().groupTabs();

        // Go back to PageStation for InitialStateRule to reset
        PageStation previousPage = tabSwitcher.leaveHubToPreviousTabViaBack();
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    public void testCreateTabGroupOf2() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        NewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
        int secondTabId = secondPage.getLoadedTab().getId();
        HubTabSwitcherStation tabSwitcher = secondPage.openHub(HubTabSwitcherStation.class);
        HubTabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        editor.openAppMenuWithEditor().groupTabs();

        // Go back to PageStation for InitialStateRule to reset
        PageStation previousPage = tabSwitcher.leaveHubToPreviousTabViaBack();
        assertFinalDestination(previousPage);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testCreateTabGroupOf2_parity() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        int firstTabId = firstPage.getLoadedTab().getId();
        NewTabPageStation secondPage = firstPage.openRegularTabAppMenu().openNewTab();
        int secondTabId = secondPage.getLoadedTab().getId();
        HubTabSwitcherStation tabSwitcher = secondPage.openHub(HubTabSwitcherStation.class);
        HubTabSwitcherListEditorFacility editor = tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);

        HubNewTabGroupDialogFacility dialog =
                editor.openAppMenuWithEditor().groupTabsWithParityEnabled();
        dialog = dialog.inputName("test_tab_group_name");
        dialog = dialog.pickColor(TabGroupColorId.RED);
        dialog.pressDone();

        // Go back to PageStation for InitialStateRule to reset
        PageStation previousPage = tabSwitcher.leaveHubToPreviousTabViaBack();
        assertFinalDestination(previousPage);
    }
}
