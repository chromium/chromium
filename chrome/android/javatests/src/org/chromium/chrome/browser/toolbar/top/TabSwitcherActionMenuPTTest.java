// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
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
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @LargeTest
    public void testOpenNewTab() {
        mCtaTestRule.startOnBlankPage().openTabSwitcherActionMenu().selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getTabCountOnUiThread(getCurrentTabModel()));
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        mCtaTestRule.startOnBlankPage().openTabSwitcherActionMenu().selectNewIncognitoTabOrWindow();
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            assertEquals(0, mCtaTestRule.tabsCount(/* incognito= */ true));
        } else {
            assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ true));
        }
    }

    @Test
    @LargeTest
    public void testOpenNewTabFromIncognito() {
        IncognitoNewTabPageStation incognitoNtp =
                mCtaTestRule
                        .startOnBlankPage()
                        .openTabSwitcherActionMenu()
                        .selectNewIncognitoTabOrWindow();

        RegularNewTabPageStation page =
                incognitoNtp.openTabSwitcherActionMenu().selectNewTabOrWindow();

        assertFalse(page.getTabModelSelector().isIncognitoSelected());
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            assertEquals(1, getTabCountOnUiThread(page.getActivity().getCurrentTabModel()));
        } else {
            assertEquals(2, getTabCountOnUiThread(page.getActivity().getCurrentTabModel()));
        }
    }

    @Test
    @LargeTest
    public void testOpenNewIncognitoTabFromIncognito() {
        IncognitoNewTabPageStation incognitoNtp =
                mCtaTestRule
                        .startOnBlankPage()
                        .openTabSwitcherActionMenu()
                        .selectNewIncognitoTabOrWindow();

        IncognitoNewTabPageStation page =
                incognitoNtp.openTabSwitcherActionMenu().selectNewIncognitoTab();

        assertTrue(page.getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getTabCountOnUiThread(page.getActivity().getCurrentTabModel()));
    }

    @Test
    @LargeTest
    public void testCloseTab() {
        WebPageStation page = mCtaTestRule.startOnBlankPage();

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = page.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher = actionMenu.selectCloseTabAndDisplayTabSwitcher();

        assertEquals(0, getTabCountOnUiThread(getCurrentTabModel()));
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();

        IncognitoNewTabPageStation incognitoNtp = blankPage.openNewIncognitoTabFast();
        RegularNewTabPageStation ntp = incognitoNtp.openNewTabFast();

        TabModel regularTabModel = getTabModelSelector().getModel(/* incognito= */ false);
        TabModel incognitoTabModel = getTabModelSelector().getModel(/* incognito= */ true);
        assertEquals(2, getTabCountOnUiThread(regularTabModel));
        assertEquals(1, getTabCountOnUiThread(incognitoTabModel));

        // Close second regular tab opened.
        TabSwitcherActionMenuFacility actionMenu = ntp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());

        // Close first regular tab opened.
        actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher = actionMenu.selectCloseTabAndDisplayTabSwitcher();

        // Only the incognito tab should still remain.
        assertEquals(0, getTabCountOnUiThread(regularTabModel));
        assertEquals(1, getTabCountOnUiThread(incognitoTabModel));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    @LargeTest
    public void testSwitchIntoAndOutOfIncognito() {
        // Open 1 regular and 1 incognito tab.
        CtaPageStation blankPage = mCtaTestRule.startOnBlankPage();
        CtaPageStation incognitoNtp = blankPage.openNewIncognitoTabFast();

        // Open action menu and switch out of incognito.
        TabSwitcherActionMenuFacility actionMenu = incognitoNtp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectSwitchOutOfIncognito(WebPageStation.newBuilder());

        // Open action menu and switch to incognito.
        actionMenu = blankPage.openTabSwitcherActionMenu();
        incognitoNtp = actionMenu.selectSwitchToIncognito(IncognitoNewTabPageStation.newBuilder());
    }

    private TabModelSelector getTabModelSelector() {
        return mCtaTestRule.getActivity().getTabModelSelector();
    }

    private TabModel getCurrentTabModel() {
        return mCtaTestRule.getActivity().getCurrentTabModel();
    }
}
