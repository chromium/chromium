// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.HubIncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.HubTabSwitcherBaseStation;
import org.chromium.chrome.test.transit.HubTabSwitcherStation;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.transit.TabThumbnailsCapturedFacility;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.IOException;
import java.util.concurrent.ExecutionException;

/** Tests for the {@link TabSwitcherLayout}. */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Batch(Batch.PER_CLASS)
public class TabSwitcherLayoutPTTest {

    private static final String TEST_URL = "/chrome/test/data/android/google.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    private static EmbeddedTestServer sTestServer;

    private PageStation mStartPage;

    @Before
    public void setUp() throws ExecutionException {
        // After setUp, Chrome is launched and has one NTP.
        mStartPage = mInitialStateRule.startOnBlankPage();

        sActivityTestRule
                .getActivity()
                .getTabContentManager()
                .setCaptureMinRequestTimeForTesting(0);
    }

    @BeforeClass
    public static void setUpClass() throws ExecutionException {
        sTestServer = sActivityTestRule.getTestServer();
    }

    /**
     * Enters the HubTabSwitcher. Like {@link TabUiTestHelper#enterTabSwitcher}, but make sure all
     * tabs have thumbnail.
     */
    private <T extends HubTabSwitcherBaseStation> T enterHTSWithThumbnailChecking(
            PageStation currentStation, Class<T> destinationType) {
        T tabSwitcherStation = currentStation.openHub(destinationType);
        TabThumbnailsCapturedFacility.waitForThumbnailsCaptured(
                tabSwitcherStation,
                destinationType.isAssignableFrom(HubIncognitoTabSwitcherStation.class));
        return tabSwitcherStation;
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Will enable tests in a followup cl.")
    public void testRenderGrid_10WebTabs() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        PageStation pageStation = Journeys.prepareTabs(mStartPage, 10, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        HubTabSwitcherStation tabSwitcherStation =
                enterHTSWithThumbnailChecking(pageStation, HubTabSwitcherStation.class);
        pageStation = tabSwitcherStation.selectTabAtIndex(0);

        tabSwitcherStation = pageStation.openHub(HubTabSwitcherStation.class);
        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "10_web_tabs");
        tabSwitcherStation.leaveHubToPreviousTabViaBack();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Will enable tests in a followup cl.")
    public void testRenderGrid_10WebTabs_InitialScroll() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        PageStation pageStation = Journeys.prepareTabs(mStartPage, 10, 0, "about:blank");
        assertEquals(9, cta.getTabModelSelector().getCurrentModel().index());
        HubTabSwitcherStation tabSwitcherStation =
                enterHTSWithThumbnailChecking(pageStation, HubTabSwitcherStation.class);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "10_web_tabs-select_last");
        tabSwitcherStation.leaveHubToPreviousTabViaBack();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Will enable tests in a followup cl.")
    public void testRenderGrid_3WebTabs() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        PageStation pageStation = Journeys.prepareTabs(mStartPage, 3, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        HubTabSwitcherStation tabSwitcherStation =
                enterHTSWithThumbnailChecking(pageStation, HubTabSwitcherStation.class);

        pageStation = tabSwitcherStation.selectTabAtIndex(0);

        tabSwitcherStation =
                enterHTSWithThumbnailChecking(pageStation, HubTabSwitcherStation.class);

        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "3_web_tabs");
        tabSwitcherStation.leaveHubToPreviousTabViaBack();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Will enable tests in a followup cl.")
    public void testRenderGrid_3WebTabs_ThumbnailCacheRefactor() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        PageStation pageStation =
                Journeys.prepareTabs(mStartPage, 3, 0, sTestServer.getURL(TEST_URL));
        // Make sure all thumbnails are there before switching tabs.
        HubTabSwitcherStation tabSwitcherStation =
                enterHTSWithThumbnailChecking(pageStation, HubTabSwitcherStation.class);
        pageStation = tabSwitcherStation.selectTabAtIndex(0);

        tabSwitcherStation = pageStation.openHub(HubTabSwitcherStation.class);

        mRenderTestRule.render(
                cta.findViewById(R.id.pane_frame), "3_web_tabs_thumbnail_cache_refactor");

        tabSwitcherStation.leaveHubToPreviousTabViaBack();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "Will enable tests in a followup cl.")
    public void testRenderGrid_Incognito() throws IOException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        // Prepare some incognito tabs and enter tab switcher.
        PageStation pageStation = Journeys.prepareTabs(mStartPage, 1, 3, "about:blank");
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        HubIncognitoTabSwitcherStation tabSwitcherStation =
                enterHTSWithThumbnailChecking(pageStation, HubIncognitoTabSwitcherStation.class);
        pageStation = tabSwitcherStation.selectTabAtIndex(0);
        tabSwitcherStation = pageStation.openHub(HubIncognitoTabSwitcherStation.class);
        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.pane_frame));
        mRenderTestRule.render(cta.findViewById(R.id.pane_frame), "3_incognito_web_tabs");
        tabSwitcherStation.leaveHubToPreviousTabViaBack();
    }
}
