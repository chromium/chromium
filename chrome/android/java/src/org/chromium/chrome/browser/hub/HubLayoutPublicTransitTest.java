// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_REFACTOR;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_RETURN_TIME;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BasePageStation;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
import org.chromium.chrome.test.transit.HubIncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.HubTabSwitcherAppMenuFacility;
import org.chromium.chrome.test.transit.HubTabSwitcherStation;
import org.chromium.chrome.test.transit.NewTabPageStation;
import org.chromium.chrome.test.transit.PageAppMenuFacility;
import org.chromium.chrome.test.transit.PageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Public transit instrumentation/integration test of Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ANDROID_HUB, START_SURFACE_REFACTOR})
@Batch(Batch.PER_CLASS)
public class HubLayoutPublicTransitTest {
    @Rule
    public BatchedPublicTransitRule<BasePageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(BasePageStation.class);

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivityPublicTransitEntryPoints mTransitEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelector selector =
                            sActivityTestRule.getActivity().getTabModelSelector();
                    assertEquals(1, selector.getModel(false).getCount());
                    assertEquals(0, selector.getModel(true).getCount());
                });
    }

    @Test
    @LargeTest
    public void testEnterAndExitHub() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        HubTabSwitcherStation tabSwitcher = page.openHub(HubTabSwitcherStation.class);

        PageStation previousTab = tabSwitcher.leaveHubToPreviousTabViaBack();

        assertFinalDestination(previousTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        HubTabSwitcherStation tabSwitcher = page.openHub(HubTabSwitcherStation.class);

        HubTabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();

        NewTabPageStation newTab = appMenu.openNewTab();

        // Reset to original state for batching.
        tabSwitcher = newTab.openHub(HubTabSwitcherStation.class);
        tabSwitcher = tabSwitcher.closeTabAtIndex(1, HubTabSwitcherStation.class);
        BasePageStation blankTab = tabSwitcher.selectTabAtIndex(0);
        assertFinalDestination(blankTab);
    }

    @Test
    @LargeTest
    public void testEnterHubAndLeaveViaAppMenuNewIncognitoTab() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        HubTabSwitcherStation tabSwitcher = page.openHub(HubTabSwitcherStation.class);

        HubTabSwitcherAppMenuFacility appMenu = tabSwitcher.openAppMenu();

        NewTabPageStation newIncognitoTab = appMenu.openNewIncognitoTab();

        // Reset to original state for batching.
        HubIncognitoTabSwitcherStation incognitoTabSwitcher =
                newIncognitoTab.openHub(HubIncognitoTabSwitcherStation.class);
        tabSwitcher = incognitoTabSwitcher.closeTabAtIndex(0, HubTabSwitcherStation.class);
        BasePageStation blankTab = tabSwitcher.selectTabAtIndex(0);
        assertFinalDestination(blankTab);
    }

    @Test
    @LargeTest
    public void testChangeTabSwitcherPanes() {
        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        PageAppMenuFacility appMenu = page.openAppMenu();
        NewTabPageStation incognitoNewTabPage = appMenu.openNewIncognitoTab();

        HubIncognitoTabSwitcherStation incognitoTabSwitcher =
                incognitoNewTabPage.openHub(HubIncognitoTabSwitcherStation.class);
        assertEquals(
                incognitoTabSwitcher,
                incognitoTabSwitcher.selectPane(
                        PaneId.INCOGNITO_TAB_SWITCHER, HubIncognitoTabSwitcherStation.class));

        HubTabSwitcherStation tabSwitcher =
                incognitoTabSwitcher.selectPane(PaneId.TAB_SWITCHER, HubTabSwitcherStation.class);

        // Reset to original state for batching.
        incognitoTabSwitcher =
                tabSwitcher.selectPane(
                        PaneId.INCOGNITO_TAB_SWITCHER, HubIncognitoTabSwitcherStation.class);
        tabSwitcher = incognitoTabSwitcher.closeTabAtIndex(0, HubTabSwitcherStation.class);
        BasePageStation blankTab = tabSwitcher.selectTabAtIndex(0);
        assertFinalDestination(blankTab);
    }

    @Test
    @LargeTest
    @EnableFeatures({SHOW_NTP_AT_STARTUP_ANDROID, START_SURFACE_RETURN_TIME})
    public void testExitHubOnStartSurfaceAsNtp() {
        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS.setForTesting(0);

        BasePageStation page = mTransitEntryPoints.startOnBlankPageBatched(mBatchedRule);

        PageAppMenuFacility appMenu = page.openAppMenu();
        NewTabPageStation newTabPage = appMenu.openNewTab();

        HubTabSwitcherStation tabSwitcher = newTabPage.openHub(HubTabSwitcherStation.class);
        page = tabSwitcher.selectTabAtIndex(0);

        tabSwitcher = page.openHub(HubTabSwitcherStation.class);
        newTabPage = pauseAndResumeActivity(tabSwitcher);

        // Reset to original state for batching.
        tabSwitcher = newTabPage.openHub(HubTabSwitcherStation.class);
        tabSwitcher = tabSwitcher.closeTabAtIndex(1, HubTabSwitcherStation.class);
        page = tabSwitcher.selectTabAtIndex(0);
        assertFinalDestination(page);
    }

    private NewTabPageStation pauseAndResumeActivity(TransitStation currentStation) {
        NewTabPageStation destination =
                new NewTabPageStation(
                        sActivityTestRule, /* incognito= */ false, /* isOpeningTab= */ false);
        return Trip.travelSync(
                currentStation,
                destination,
                (t) -> {
                    ChromeTabbedActivity cta = sActivityTestRule.getActivity();
                    ChromeApplicationTestUtils.fireHomeScreenIntent(cta);
                    try {
                        sActivityTestRule.resumeMainActivityFromLauncher();
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
    }
}
