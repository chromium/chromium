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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BasePageStation;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.HubIncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.HubTabSwitcherStation;
import org.chromium.chrome.test.transit.PageAppMenuFacility;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.TabSwitcherActionMenuFacility;

/**
 * Instrumentation tests for tab switcher long-press menu popup.
 *
 * <p>Batched version of TabSwitcherActionMenuPTTest.
 *
 * <p>This class is the updated version of TabSwitcherActionMenuBatchedPTTest with Hub enabled. To
 * better serve as a batching test case, I'm keeping the two separate as two 4-case test batches
 * instead of three 2-case test batches.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ANDROID_HUB)
@Batch(Batch.PER_CLASS)
public class TabSwitcherActionMenuWithHubBatchedPTTest {

    @Rule
    public BatchedPublicTransitRule<BasePageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(BasePageStation.class);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(mActivityTestRule);

    @Test
    @LargeTest
    public void testCloseTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        HubTabSwitcherStation tabSwitcher = actionMenu.selectCloseTab(HubTabSwitcherStation.class);

        assertEquals(0, getCurrentTabModel().getCount());

        page = tabSwitcher.openNewTab();
        assertFinalDestination(page);
    }

    @Test
    @LargeTest
    public void testOpenNewTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        // Opening a new tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getCurrentTabModel().getCount());

        // Return to one non-incognito blank tab
        actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectCloseTab(BasePageStation.class);
        assertFinalDestination(page);
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        // Opening a new incognito tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectNewIncognitoTab();

        assertTrue(getTabModelSelector().isIncognitoSelected());
        assertEquals(1, getCurrentTabModel().getCount());

        // Return to one non-incognito blank tab
        actionMenu = page.openTabSwitcherActionMenu();
        page = actionMenu.selectCloseTab(BasePageStation.class);
        assertFinalDestination(page);
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

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
        HubTabSwitcherStation regularTabSwitcher =
                actionMenu.selectCloseTab(HubTabSwitcherStation.class);

        // Only the incognito tab should still remain.
        assertEquals(0, regularTabModel.getCount());
        assertEquals(1, incognitoTabModel.getCount());

        // Return to one non-incognito blank tab
        HubIncognitoTabSwitcherStation incognitoTabSwitcher =
                regularTabSwitcher.selectIncognitoTabList();
        regularTabSwitcher = incognitoTabSwitcher.closeTabAtIndex(0, HubTabSwitcherStation.class);
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
