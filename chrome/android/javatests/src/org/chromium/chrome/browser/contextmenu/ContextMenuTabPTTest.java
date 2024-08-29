// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.page.PageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUiFacility;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;

/**
 * Public transit test for opening new tabs using context menu, and test the interactions around the
 * relevant UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION
})
@Batch(Batch.PER_CLASS)
public class ContextMenuTabPTTest {

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @BeforeClass
    public static void setupBeforeClass() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
    }

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    /**
     * Assert open tab in new tab group. This test is greatly in common with
     * ContextMenuTest#testOpenLinksInNewTabsAndVerifyTabIndexOrdering.
     */
    @Test
    @MediumTest
    public void openNewTabFromContextMenu() {
        PageStation blankPage = mInitialStateRule.startOnBlankPage();

        TopBottomLinksPageStation contextMenuPage =
                TopBottomLinksPageStation.loadPage(sActivityTestRule, blankPage);
        contextMenuPage.openContextMenuOnTopLink().openInNewTab();
        assertFinalDestination(contextMenuPage);
    }

    /** Assert open tab in new tab group. */
    @Test
    @MediumTest
    public void openNewTabInGroupFromContextMenu() {
        PageStation blankPage = mInitialStateRule.startOnBlankPage();

        TopBottomLinksPageStation contextMenuPage =
                TopBottomLinksPageStation.loadPage(sActivityTestRule, blankPage);
        TabGroupUiFacility<WebPageStation> tabGroupUi =
                contextMenuPage.openContextMenuOnTopLink().openTabInNewGroup();
        assertFinalDestination(contextMenuPage, tabGroupUi);
    }
}
