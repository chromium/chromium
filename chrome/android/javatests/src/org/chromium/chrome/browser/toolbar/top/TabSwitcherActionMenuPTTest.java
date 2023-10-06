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
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.AppMenuFacility;
import org.chromium.chrome.test.transit.BasePageStation;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.TabSwitcherActionMenuFacility;
import org.chromium.chrome.test.transit.TabSwitcherStation;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Instrumentation tests for tab switcher long-press menu popup.
 *
 * <p>Public Transit version of TabSwitcherActionMenuTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class TabSwitcherActionMenuPTTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mActivityTestRule);

    @Test
    @LargeTest
    public void testCloseTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPage();

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        TabSwitcherStation tabSwitcherStation = actionMenu.selectCloseTab(TabSwitcherStation.class);

        assertEquals(0, getCurrentTabModel().getCount());
        assertFinalDestination(tabSwitcherStation);
    }

    @Test
    @LargeTest
    public void testOpenNewTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPage();

        // Opening a new tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getCurrentTabModel().getCount());
        assertFinalDestination(page);
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPage();

        // Opening a new incognito tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectNewIncognitoTab();

        assertTrue(getTabModelSelector().isIncognitoSelected());
        assertEquals(1, getCurrentTabModel().getCount());
        assertFinalDestination(page);
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPage();

        AppMenuFacility appMenu = page.openAppMenu();
        page = appMenu.openNewIncognitoTab();

        appMenu = page.openAppMenu();
        page = appMenu.openNewTab();

        TabModel regularTabModel = getTabModelSelector().getModel(/* incognito= */ false);
        TabModel incognitoTabModel = getTabModelSelector().getModel(/* incognito= */ true);
        assertEquals(2, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());

        // Close second regular tab opened.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectCloseTab(PageStation.class);

        // Close first regular tab opened.
        actionMenu = page.openTabSwitcherActionMenu();
        TabSwitcherStation tabSwitcher = actionMenu.selectCloseTab(TabSwitcherStation.class);

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
