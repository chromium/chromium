// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.TabSwitcherActionMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;

/**
 * Instrumentation tests for tab switcher long-press menu popup.
 *
 * <p>Public Transit version of TabSwitcherActionMenuTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason =
                "Example for Public Transit tests. TabSwitcherActionMenuBatchedPTTest is the"
                        + " batched example.")
public class TabSwitcherActionMenuPTTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mActivityTestRule);

    @Test
    @LargeTest
    public void testOpenNewTab() {
        WebPageStation page = mTransitEntryPoints.startOnBlankPageNonBatched();

        // Opening a new tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        RegularNewTabPageStation ntp = actionMenu.selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getCurrentTabModel().getCount());
        assertFinalDestination(ntp);
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        WebPageStation page = mTransitEntryPoints.startOnBlankPageNonBatched();

        // Opening a new incognito tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        IncognitoNewTabPageStation incognitoNtp = actionMenu.selectNewIncognitoTab();

        assertTrue(getTabModelSelector().isIncognitoSelected());
        assertEquals(1, getCurrentTabModel().getCount());
        assertFinalDestination(incognitoNtp);
    }

    @Test
    @LargeTest
    public void testCloseTab() {
        WebPageStation page = mTransitEntryPoints.startOnBlankPageNonBatched();

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher = actionMenu.selectCloseTabAndDisplayTabSwitcher();

        assertEquals(0, getCurrentTabModel().getCount());
        assertFinalDestination(tabSwitcher);
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        WebPageStation blankPage = mTransitEntryPoints.startOnBlankPageNonBatched();

        IncognitoNewTabPageStation incognitoNtp =
                blankPage.openGenericAppMenu().openNewIncognitoTab();
        RegularNewTabPageStation ntp = incognitoNtp.openGenericAppMenu().openNewTab();

        TabModel regularTabModel = getTabModelSelector().getModel(/* incognito= */ false);
        TabModel incognitoTabModel = getTabModelSelector().getModel(/* incognito= */ true);
        assertEquals(2, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());

        // Close second regular tab opened.
        TabSwitcherActionMenuFacility actionMenu = ntp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());

        // Close first regular tab opened.
        actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher = actionMenu.selectCloseTabAndDisplayTabSwitcher();

        // Only the incognito tab should still remain.
        assertEquals(0, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());
        assertFinalDestination(tabSwitcher);
    }

    private TabModelSelector getTabModelSelector() {
        return mActivityTestRule.getActivity().getTabModelSelector();
    }

    private TabModel getCurrentTabModel() {
        return mActivityTestRule.getActivity().getCurrentTabModel();
    }
}
