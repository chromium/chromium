// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUiFacility;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.ui.base.DeviceFormFactor;

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
// On tablets, the context is showing using a popup window. This currently doesn't work well with
// the espresso matching in the public transit framework. See https://crbug.com/363047177.
@Restriction(DeviceFormFactor.PHONE)
public class ContextMenuTabPTTest {
    @BeforeClass
    public static void setupBeforeClass() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
    }

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    /**
     * Assert open tab in new tab group. This test is greatly in common with
     * ContextMenuTest#testOpenLinksInNewTabsAndVerifyTabIndexOrdering.
     */
    @Test
    @MediumTest
    public void openNewTabFromContextMenu() {
        CtaPageStation blankPage = mCtaTestRule.startOnBlankPage();

        var topBottomLinkPageAndTop =
                TopBottomLinksPageStation.loadPage(mCtaTestRule.getActivityTestRule(), blankPage);
        TopBottomLinksPageStation contextMenuPage = topBottomLinkPageAndTop.first;
        TopBottomLinksPageStation.TopFacility topFacility = topBottomLinkPageAndTop.second;

        topFacility.openContextMenuOnTopLink().openInNewTab();
        assertFinalDestination(contextMenuPage);
    }

    /** Assert open tab in new tab group. */
    @Test
    @MediumTest
    public void openNewTabInGroupFromContextMenu() {
        CtaPageStation blankPage = mCtaTestRule.startOnBlankPage();

        var topBottomLinkPageAndTop =
                TopBottomLinksPageStation.loadPage(mCtaTestRule.getActivityTestRule(), blankPage);
        TopBottomLinksPageStation contextMenuPage = topBottomLinkPageAndTop.first;
        TopBottomLinksPageStation.TopFacility topFacility = topBottomLinkPageAndTop.second;

        TabGroupUiFacility<WebPageStation> tabGroupUi =
                topFacility.openContextMenuOnTopLink().openTabInNewGroup();
        assertFinalDestination(contextMenuPage, tabGroupUi);
    }
}
