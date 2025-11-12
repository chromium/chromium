// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.when;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;

import java.io.IOException;

/** Public transit tests for the Tab Group Dialog representing tab groups. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO(https://crbug.com/392634251): Fix line height when elegant text height is used with Roboto
// or enable Google Sans (Text) in //chrome/ tests on Android T+.
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@DisableFeatures({
    ChromeFeatureList.ANDROID_ELEGANT_TEXT_HEIGHT,
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE
})
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabGroupDialogPTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .build();

    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;

    @Before
    public void setUp() {
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNewTabCreation() throws IOException {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        firstPage, 3, 0, "about:blank", WebPageStation::newBuilder);

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        mRenderTestRule.render(
                mCtaTestRule.getActivity().findViewById(R.id.dialog_container_view),
                "tab_grid_dialog-normal_mode");

        RegularNewTabPageStation secondPage = tabGroupDialogFacility.openNewRegularTab();

        // Assert we have gone back to PageStation for InitialStateRule to reset
        assertFinalDestination(secondPage);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testIncognitoNewTabCreation() throws IOException {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        firstPage, 1, 3, "about:blank", WebPageStation::newBuilder);

        IncognitoTabSwitcherStation tabSwitcher = pageStation.openIncognitoTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        mRenderTestRule.render(
                mCtaTestRule.getActivity().findViewById(R.id.dialog_container_view),
                "tab_grid_dialog-incognito_mode");

        IncognitoNewTabPageStation secondPage = tabGroupDialogFacility.openNewIncognitoTab();

        // Assert we have gone back to PageStation for InitialStateRule to reset
        assertFinalDestination(secondPage);
    }

    @Test
    @MediumTest
    public void testTabGroupNameChange() {
        WebPageStation firstPage = mCtaTestRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabs(firstPage, 3, 0, "about:blank", WebPageStation::newBuilder);

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        tabGroupDialogFacility =
                tabGroupDialogFacility.inputName("test_tab_group_name");
        tabGroupDialogFacility.pressBackArrowToExit();

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }
}
