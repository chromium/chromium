// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherTabCardContextMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.util.concurrent.ExecutionException;

/** Tests for Tab Switcher Card context menus. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE
})
@DisabledTest(message = "Flaky. See crbug.com/467341609")
@Batch(Batch.PER_CLASS)
public class TabSwitcherCardContextMenuTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mFirstPage;

    @Before
    public void setUp() throws ExecutionException {
        // After setUp, Chrome is launched and has one NTP.
        mFirstPage = mCtaTestRule.startOnBlankPage();

        ChromeTabbedActivity cta = mCtaTestRule.getActivity();

        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @MediumTest
    public void testTabCardMenuInTabSwitcher_addToNewGroup() {
        Tab firstTab = mFirstPage.loadedTabElement.value();
        @TabId int firstTabId = firstTab.getId();

        RegularTabSwitcherStation tabSwitcher = mFirstPage.openRegularTabSwitcher();

        tabSwitcher
                .expectTabCard(firstTabId, firstTab.getTitle())
                .showContextMenu()
                .clickAddTabToNewGroup()
                .pressDone();

        RegularNewTabPageStation ntp = tabSwitcher.openNewTab();
        assertFinalDestination(ntp);
    }

    @Test
    @MediumTest
    public void testTabCardMenuInTabSwitcher_addToGroup() {
        Tab firstTab = mFirstPage.loadedTabElement.value();
        @TabId int firstTabId = firstTab.getId();

        RegularTabSwitcherStation tabSwitcher = mFirstPage.openRegularTabSwitcher();

        tabSwitcher
                .expectTabCard(firstTabId, firstTab.getTitle())
                .showContextMenu()
                .clickAddTabToNewGroup()
                .pressDone();

        RegularNewTabPageStation ntp = tabSwitcher.openNewTab();
        Tab secondTab = ntp.loadedTabElement.value();

        tabSwitcher = ntp.openRegularTabSwitcher();
        tabSwitcher
                .expectTabCard(secondTab.getId(), secondTab.getTitle())
                .showContextMenu()
                .clickAddTabToGroup(/* isNewTabGroupRowVisible= */ true)
                .clickNewTabGroupRow()
                .inputName("TestGroup")
                .pressDone();

        // Verify the tabs are in two distinct tab groups.
        assertNotNull(firstTab.getTabGroupId());
        assertNotNull(secondTab.getTabGroupId());
        assertNotEquals(firstTab.getTabGroupId(), secondTab.getTabGroupId());

        ntp = tabSwitcher.openNewTab();
        assertFinalDestination(ntp);
    }

    @Test
    @MediumTest
    public void testTabCardMenuInTabSwitcher_shareIsAbsentForNtp() {
        RegularNewTabPageStation ntp = mFirstPage.openRegularTabSwitcher().openNewTab();
        Tab secondTab = ntp.loadedTabElement.value();
        @TabId int secondTabId = secondTab.getId();

        RegularTabSwitcherStation tabSwitcher = ntp.openRegularTabSwitcher();

        TabSwitcherTabCardContextMenuFacility<TabSwitcherStation> contextMenu =
                tabSwitcher.expectTabCard(secondTabId, secondTab.getTitle()).showContextMenu();
        contextMenu.share.checkAbsent();
        contextMenu.pressBackTo().exitFacility();

        ntp = tabSwitcher.openNewTab();
        assertFinalDestination(ntp);
    }
}
