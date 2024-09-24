// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.LargeTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.TabSwitcherActionMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Instrumentation tests for tab switcher long-press menu popup.
 *
 * <p>Batched version of TabSwitcherActionMenuPTTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@ImportantFormFactors(DeviceFormFactor.TABLET)
public class TabSwitcherActionMenuBatchedPTTest {

    @Rule
    public BatchedPublicTransitRule<WebPageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(WebPageStation.class, /* expectResetByTest= */ true);

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);

    @Test
    @LargeTest
    public void testCloseTab() {
        WebPageStation blankPage = mTransitEntryPoints.startOnBlankPage(mBatchedRule);

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher = actionMenu.selectCloseTabAndDisplayTabSwitcher();

        assertEquals(0, getCurrentTabModel().getCount());

        blankPage = tabSwitcher.openNewTab().loadAboutBlank();
        assertFinalDestination(blankPage);
    }

    @Test
    @LargeTest
    public void testOpenNewTab() {
        WebPageStation blankPage = mTransitEntryPoints.startOnBlankPage(mBatchedRule);

        // Opening a new tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularNewTabPageStation ntp = actionMenu.selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getCurrentTabModel().getCount());

        // Return to one non-incognito blank tab
        actionMenu = ntp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());
        assertFinalDestination(blankPage);
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        WebPageStation blankPage = mTransitEntryPoints.startOnBlankPage(mBatchedRule);

        // Opening a new incognito tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = blankPage.openTabSwitcherActionMenu();
        IncognitoNewTabPageStation incognitoNtp = actionMenu.selectNewIncognitoTab();

        assertTrue(getTabModelSelector().isIncognitoSelected());
        assertEquals(1, getCurrentTabModel().getCount());

        // Return to one non-incognito blank tab
        actionMenu = incognitoNtp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayRegularTab(WebPageStation.newBuilder());
        assertFinalDestination(blankPage);
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        WebPageStation blankPage = mTransitEntryPoints.startOnBlankPage(mBatchedRule);

        IncognitoNewTabPageStation incognitoNtp =
                blankPage.openRegularTabAppMenu().openNewIncognitoTab();
        RegularNewTabPageStation ntp = incognitoNtp.openAppMenu().openNewTab();

        TabModel regularTabModel = getTabModelSelector().getModel(/* incognito= */ false);
        TabModel incognitoTabModel = getTabModelSelector().getModel(/* incognito= */ true);
        assertEquals(2, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());

        // Close second regular tab opened.
        TabSwitcherActionMenuFacility actionMenu = ntp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());

        // Close first regular tab opened.
        actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularTabSwitcherStation regularTabSwitcher =
                actionMenu.selectCloseTabAndDisplayTabSwitcher();

        // Only the incognito tab should still remain.
        assertEquals(0, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());

        // Return to one non-incognito blank tab
        IncognitoTabSwitcherStation incognitoTabSwitcher =
                regularTabSwitcher.selectIncognitoTabList();
        regularTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        blankPage = regularTabSwitcher.openNewTab().loadAboutBlank();
        assertFinalDestination(blankPage);
    }

    private TabModelSelector getTabModelSelector() {
        return sActivityTestRule.getActivity().getTabModelSelector();
    }

    private TabModel getCurrentTabModel() {
        return sActivityTestRule.getActivity().getCurrentTabModel();
    }
}
