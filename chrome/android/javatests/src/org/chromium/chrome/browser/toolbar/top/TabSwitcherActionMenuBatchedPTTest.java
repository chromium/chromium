// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
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
@EnableFeatures(ChromeFeatureList.TAB_STRIP_INCOGNITO_MIGRATION)
@DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
@Batch(Batch.PER_CLASS)
@ImportantFormFactors(DeviceFormFactor.ONLY_TABLET)
@Restriction(DeviceFormFactor.PHONE_OR_TABLET)
public class TabSwitcherActionMenuBatchedPTTest {
    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mCtaTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    @Test
    @LargeTest
    public void testCloseTab() {
        WebPageStation blankPage = mCtaTestRule.start();

        // Closing the only tab should lead to the Tab Switcher.
        TabSwitcherActionMenuFacility actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularTabSwitcherStation tabSwitcher = actionMenu.selectCloseTabAndDisplayTabSwitcher();

        assertEquals(0, getTabCountOnUiThread(getCurrentTabModel()));

        blankPage = tabSwitcher.openNewTab().loadAboutBlank();
        assertFinalDestination(blankPage);
    }

    @Test
    @LargeTest
    public void testOpenNewTab() {
        WebPageStation blankPage = mCtaTestRule.start();

        // Opening a new tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = blankPage.openTabSwitcherActionMenu();
        RegularNewTabPageStation ntp = actionMenu.selectNewTab();

        assertFalse(getTabModelSelector().isIncognitoSelected());
        assertEquals(2, getTabCountOnUiThread(getCurrentTabModel()));

        // Return to one non-incognito blank tab
        actionMenu = ntp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());
        assertFinalDestination(blankPage);
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOpenNewIncognitoTab() {
        WebPageStation blankPage = mCtaTestRule.start();

        // Opening a new incognito tab should display it on the screen.
        TabSwitcherActionMenuFacility actionMenu = blankPage.openTabSwitcherActionMenu();
        IncognitoNewTabPageStation incognitoNtp = actionMenu.selectNewIncognitoTab();

        assertTrue(getTabModelSelector().isIncognitoSelected());
        assertEquals(1, getTabCountOnUiThread(getCurrentTabModel()));

        // Return to one non-incognito blank tab
        actionMenu = incognitoNtp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayRegularTab(WebPageStation.newBuilder());
        assertFinalDestination(blankPage);
    }

    /** Regression test for crbug.com/1448791 */
    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testClosingAllRegularTabs_DoNotFinishActivity() {
        WebPageStation blankPage = mCtaTestRule.start();

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
        RegularTabSwitcherStation regularTabSwitcher =
                actionMenu.selectCloseTabAndDisplayTabSwitcher();

        // Only the incognito tab should still remain.
        assertEquals(0, getTabCountOnUiThread(regularTabModel));
        assertEquals(1, getTabCountOnUiThread(incognitoTabModel));

        // Return to one non-incognito blank tab
        IncognitoTabSwitcherStation incognitoTabSwitcher =
                regularTabSwitcher.selectIncognitoTabsPane();
        regularTabSwitcher =
                incognitoTabSwitcher.closeTabAtIndex(0, RegularTabSwitcherStation.class);
        blankPage = regularTabSwitcher.openNewTab().loadAboutBlank();
        assertFinalDestination(blankPage);
    }

    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testSwitchIntoAndOutOfIncognito() {
        // Open 1 regular and 1 incognito tab.
        WebPageStation blankPage = mCtaTestRule.start();
        CtaPageStation incognitoNtp = blankPage.openNewIncognitoTabFast();

        // Open action menu and switch out of incognito.
        TabSwitcherActionMenuFacility actionMenu = incognitoNtp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectSwitchOutOfIncognito(WebPageStation.newBuilder());

        // Open action menu and switch into incognito.
        actionMenu = blankPage.openTabSwitcherActionMenu();
        incognitoNtp = actionMenu.selectSwitchToIncognito(IncognitoNewTabPageStation.newBuilder());

        // Return to regular blank tab
        actionMenu = incognitoNtp.openTabSwitcherActionMenu();
        blankPage = actionMenu.selectCloseTabAndDisplayRegularTab(WebPageStation.newBuilder());
        assertFinalDestination(blankPage);
    }

    private TabModelSelector getTabModelSelector() {
        return mCtaTestRule.getActivity().getTabModelSelector();
    }

    private TabModel getCurrentTabModel() {
        return mCtaTestRule.getActivity().getCurrentTabModel();
    }
}
