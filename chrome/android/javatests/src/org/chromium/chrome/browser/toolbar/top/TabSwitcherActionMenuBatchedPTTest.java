// Copyright 2024 The Chromium Authors
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

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.PageAppMenuFacility;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.TabSwitcherActionMenuFacility;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Instrumentation tests for tab switcher long-press menu popup.
 *
 * <p>Batched version of TabSwitcherActionMenuPTTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB)
@Batch(Batch.PER_CLASS)
public class TabSwitcherActionMenuBatchedPTTest {

    @Rule
    public BatchedPublicTransitRule<PageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(PageStation.class);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mActivityTestRule);

    @Test
    @LargeTest
    public void testCloseTab() {
        PageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher =
                actionMenu.selectCloseTab(RegularTabSwitcherStation.class);

        assertEquals(0, getCurrentTabModel().getCount());

        page = tabSwitcher.openNewTab();
        assertFinalDestination(page);
    }

    @Test
    @LargeTest
    public void testOpenNewTab() {
        PageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        // Opening a new tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getCurrentTabModel().getCount());

        // Return to one non-incognito blank tab
        actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectCloseTab(PageStation.class);
        assertFinalDestination(page);
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        PageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        // Opening a new incognito tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectNewIncognitoTab();

        assertTrue(getTabModelSelector().isIncognitoSelected());
        assertEquals(1, getCurrentTabModel().getCount());

        // Return to one non-incognito blank tab
        actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectCloseTab(PageStation.class);
        assertFinalDestination(page);
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        PageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        PageAppMenuFacility appMenu = page.openAppMenu();
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
        RegularTabSwitcherStation regularTabSwitcher =
                actionMenu.selectCloseTab(RegularTabSwitcherStation.class);

        // Only the incognito tab should still remain.
        assertEquals(0, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());

        // Return to one non-incognito blank tab
        IncognitoTabSwitcherStation incognitoTabSwitcher =
                regularTabSwitcher.selectIncognitoTabList();
        regularTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        page = regularTabSwitcher.openNewTab();
        assertFinalDestination(page);
    }

    private TabModelSelector getTabModelSelector() {
        return mActivityTestRule.getActivity().getTabModelSelector();
    }

    private TabModel getCurrentTabModel() {
        return mActivityTestRule.getActivity().getCurrentTabModel();
    }
}
