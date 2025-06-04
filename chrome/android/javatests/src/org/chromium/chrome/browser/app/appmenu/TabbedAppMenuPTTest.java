// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageAppMenuFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.IncognitoWebPageAppMenuFacility;
import org.chromium.chrome.test.transit.page.RegularWebPageAppMenuFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;
import org.chromium.chrome.test.util.ChromeRenderTestRule;

import java.io.IOException;

/** Public Transit tests for the app menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabbedAppMenuPTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setDescription("App menu")
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_APP_MENU)
                    .build();

    /** Tests that "New tab" opens a new tab with the NTP. */
    @Test
    @LargeTest
    public void testOpenNewTab() {
        RegularNewTabPageStation newTabPage =
                mCtaTestRule.startOnBlankPage().openRegularTabAppMenu().openNewTab();

        assertEquals(2, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertEquals(0, mCtaTestRule.tabsCount(/* incognito= */ true));
        assertFinalDestination(newTabPage);
    }

    /** Tests that "New Incognito tab" opens a new incognito tab with the incognito NTP. */
    @Test
    @LargeTest
    public void testOpenNewIncognitoTab() {
        IncognitoNewTabPageStation newIncognitoTabPage =
                mCtaTestRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();

        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertEquals(1, mCtaTestRule.tabsCount(/* incognito= */ true));
        assertFinalDestination(newIncognitoTabPage);
    }

    /** Tests that "Settings" opens the SettingsActivity. */
    @Test
    @LargeTest
    public void testOpenSettings() {
        WebPageStation pageStation = mCtaTestRule.startOnBlankPage();
        Tab tab = pageStation.loadedTabElement.get();
        SettingsStation settings = pageStation.openRegularTabAppMenu().openSettings();

        assertFinalDestination(settings);

        // Exit settings for the initial state rule to be able to reset state.
        settings.pressBack(
                WebPageStation.newBuilder()
                        .withIncognito(false)
                        .withIsOpeningTabs(0)
                        .withTabAlreadySelected(tab)
                        .build());
    }

    /**
     * Tests that all expected items declared in NewTabPageRegularAppMenuFacility are present in the
     * app menu opened from a regular NTP.
     */
    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testNewTabPageRegularAppMenuItems() throws IOException {
        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation newTabPage = blankPage.openRegularTabAppMenu().openNewTab();
        RegularNewTabPageAppMenuFacility menu = newTabPage.openAppMenu();

        mRenderTestRule.render(menu.menuListElement.get(), "regular_ntp_app_menu_v3");
        menu.verifyPresentItems();
        assertFinalDestination(newTabPage, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Tests that all expected items declared in NewTabPageIncognitoAppMenuFacility are present in
     * the app menu opened from an incognito NTP.
     */
    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testNewTabPageIncognitoAppMenuItems() throws IOException {
        IncognitoNewTabPageStation incognitoNewTabPage =
                mCtaTestRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();
        IncognitoNewTabPageAppMenuFacility menu = incognitoNewTabPage.openAppMenu();

        mRenderTestRule.render(menu.menuListElement.get(), "incognito_ntp_app_menu");
        menu.verifyPresentItems();
        assertFinalDestination(incognitoNewTabPage, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Tests that all expected items declared in WebPageRegularAppMenuFacility are present in the
     * app menu opened from a regular Tab displaying a web page.
     */
    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testWebPageRegularAppMenuItems() throws IOException {
        WebPageStation blankPage = mCtaTestRule.startOnBlankPage();
        RegularWebPageAppMenuFacility menu = blankPage.openRegularTabAppMenu();

        mRenderTestRule.render(menu.menuListElement.get(), "regular_webpage_app_menu_v3");
        menu.verifyPresentItems();
        assertFinalDestination(blankPage, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /**
     * Tests that all expected items declared in WebPageIncognitoAppMenuFacility are present in the
     * app menu opened from an incognito Tab displaying a web page.
     */
    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testWebPageIncognitoAppMenuItems() throws IOException {
        IncognitoNewTabPageStation incognitoNtp =
                mCtaTestRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();

        WebPageStation pageOne =
                incognitoNtp.loadPageProgrammatically(
                        mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                        NavigatePageStations.newNavigateOnePageBuilder());
        IncognitoWebPageAppMenuFacility menu = pageOne.openIncognitoTabAppMenu();

        mRenderTestRule.render(menu.menuListElement.get(), "incognito_webpage_app_menu");
        menu.verifyPresentItems();
        assertFinalDestination(pageOne, menu);

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /** Tests that entering the Tab Switcher causes the app menu to close. */
    @Test
    @LargeTest
    public void testHideMenuOnToggleOverview() {
        WebPageStation page = mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mCtaTestRule.getActivity();
        LayoutManagerChrome layoutManager = activity.getLayoutManager();

        page.openRegularTabAppMenu();

        // Go to Tab Switcher programmatically because the App Menu covers the button.
        RegularTabSwitcherStation tabSwitcher =
                page.travelToSync(
                        RegularTabSwitcherStation.from(activity.getTabModelSelector()),
                        Transition.runTriggerOnUiThreadOption(),
                        () -> layoutManager.showLayout(LayoutType.TAB_SWITCHER, false));

        tabSwitcher.openAppMenu();

        // Go to a Web Page programmatically because tapping outside the app menu causes it to
        // capture the event and close.
        tabSwitcher.travelToSync(
                WebPageStation.newBuilder().initFrom(page).build(),
                Transition.runTriggerOnUiThreadOption(),
                () -> layoutManager.showLayout(LayoutType.BROWSING, false));
    }
}
