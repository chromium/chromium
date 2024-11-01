// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.edge_to_edge;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.test.transit.edge_to_edge.ViewportFitCoverPageStation.loadViewportFitCoverPage;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.edge_to_edge.EdgeToEdgeBottomChinFacility;
import org.chromium.chrome.test.transit.edge_to_edge.ViewportFitCoverPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.TopBottomLinksPageStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for edge to edge using public transit. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION
})
@EnableFeatures({
    ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR
            + ":disable_bottom_controls_stacker_y_offset/false",
    "DynamicSafeAreaInsets",
    "DynamicSafeAreaInsetsOnScroll",
    "DrawCutoutEdgeToEdge",
    ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN
})
@Batch(Batch.PER_CLASS)
// Bots <= VERSION_CODES.S use 3-bottom nav bar. See crbug.com/352402600
@MinAndroidSdkLevel(VERSION_CODES.S_V2)
@Restriction({DeviceFormFactor.PHONE, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
public class EdgeToEdgePTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Test
    @SmallTest
    public void loadViewportFitCover() {
        WebPageStation blankPage = mInitialStateRule.startOnBlankPage();
        ViewportFitCoverPageStation e2ePage =
                loadViewportFitCoverPage(sActivityTestRule, blankPage);
        TransitAsserts.assertFinalDestination(e2ePage);
    }

    /** Test that show the bottom controls by showing tab in group from context menu. */
    @Test
    @MediumTest
    public void openNewTabInGroupAtPageBottom() {
        ThreadUtils.runOnUiThread(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        WebPageStation blankPage = mInitialStateRule.startOnBlankPage();
        var topBottomLinkPageAndTop =
                TopBottomLinksPageStation.loadPage(sActivityTestRule, blankPage);
        TopBottomLinksPageStation topBottomLinkPage = topBottomLinkPageAndTop.first;
        TopBottomLinksPageStation.TopFacility topFacility = topBottomLinkPageAndTop.second;

        TopBottomLinksPageStation.BottomFacility bottomFacility = topFacility.scrollToBottom();
        var tabGroupUiFacility = bottomFacility.openContextMenuOnBottomLink().openTabInNewGroup();

        TransitAsserts.assertFinalDestination(
                topBottomLinkPage, bottomFacility, tabGroupUiFacility);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE})
    public void fromNtpToRegularPage() {
        // Start the page on NTP, chin is not visible.
        var newTabPage = mInitialStateRule.startOnNtp();
        var chinOnNtp =
                newTabPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(true), /* trigger= */ null);
        assertEquals(
                "On ntp the bottom chin should be VISIBLE_IF_OTHERS_VISIBLE.",
                LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                chinOnNtp.getBottomChin().getLayerVisibility());

        // Navigate the page to another web page, ensure the page has the chin visible.
        var pair = TopBottomLinksPageStation.loadPage(sActivityTestRule, newTabPage);
        TopBottomLinksPageStation regularPage = pair.first;
        var chinOnWebPage =
                regularPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(false), /* trigger= */ null);
        assertEquals(
                "On a regular page the bottom chin should be always VISIBLE.",
                LayerVisibility.VISIBLE,
                chinOnWebPage.getBottomChin().getLayerVisibility());

        TransitAsserts.assertFinalDestination(regularPage);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE})
    public void fromNtpToTabSwitcher() {
        // Start the page on NTP, chin is not visible.
        var newTabPage = mInitialStateRule.startOnNtp();
        var chinOnNtp =
                newTabPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(true), /* trigger= */ null);
        assertEquals(
                "On ntp the bottom chin should be VISIBLE_IF_OTHERS_VISIBLE.",
                LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                chinOnNtp.getBottomChin().getLayerVisibility());

        // Navigate the page to another web page, ensure the page has the chin visible.
        // On the hub, there's no "page", so don't set an expectation whether the page is opt-in.
        var tabSwitcher = newTabPage.openRegularTabSwitcher();
        var chinOnHub =
                tabSwitcher.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(null), /* trigger= */ null);
        assertEquals(
                "On the hub the bottom chin should be always VISIBLE_IF_OTHERS_VISIBLE.",
                LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                chinOnHub.getBottomChin().getLayerVisibility());

        TransitAsserts.assertFinalDestination(tabSwitcher);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE,
        ChromeFeatureList.EDGE_TO_EDGE_WEB_OPT_IN
    })
    public void fromNtpToOptInPage() {
        // Start the page on NTP, chin is not visible.
        var newTabPage = mInitialStateRule.startOnNtp();
        var chinOnNtp =
                newTabPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(true), /* trigger= */ null);
        assertEquals(
                "On ntp the bottom chin should be VISIBLE_IF_OTHERS_VISIBLE.",
                LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                chinOnNtp.getBottomChin().getLayerVisibility());

        // Navigate the page to another web page, ensure the page has the chin visible.
        var optInPage =
                ViewportFitCoverPageStation.loadViewportFitCoverPage(sActivityTestRule, newTabPage);
        var chinOnOptInPage =
                optInPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(true), /* trigger= */ null);
        assertEquals(
                "On a opt-in page the bottom chin should be VISIBLE_IF_OTHERS_VISIBLE.",
                LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                chinOnOptInPage.getBottomChin().getLayerVisibility());

        TransitAsserts.assertFinalDestination(optInPage);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.EDGE_TO_EDGE_WEB_OPT_IN})
    public void fromBlankPageToOptInPage() {
        // Start the page on NTP, chin is not visible.
        var blankPage = mInitialStateRule.startOnBlankPage();
        var chinOnBlankPage =
                blankPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(false), /* trigger= */ null);
        assertEquals(
                "On the blank page (not opt-in e2e), chin should be visible.",
                LayerVisibility.VISIBLE,
                chinOnBlankPage.getBottomChin().getLayerVisibility());

        // Navigate the page to another web page, ensure the page has the chin visible.
        var optInPage =
                ViewportFitCoverPageStation.loadViewportFitCoverPage(sActivityTestRule, blankPage);
        var chinOnOptInPage =
                optInPage.enterFacilitySync(
                        new EdgeToEdgeBottomChinFacility<>(true), /* trigger= */ null);
        assertEquals(
                "On a opt-in page the bottom chin should be VISIBLE_IF_OTHERS_VISIBLE.",
                LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE,
                chinOnOptInPage.getBottomChin().getLayerVisibility());

        TransitAsserts.assertFinalDestination(optInPage);
    }
}
