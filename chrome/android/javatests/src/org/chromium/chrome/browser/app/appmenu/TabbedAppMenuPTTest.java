// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ChromeTriggers;
import org.chromium.chrome.test.transit.bookmarks.BookmarksPhoneStation;
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
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;

/** Public Transit tests for the app menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO: Add new tests when the flag is enabled.
@DisableFeatures({ChromeFeatureList.ANDROID_THEME_MODULE, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
public class TabbedAppMenuPTTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(3)
                    .setDescription("App menu")
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_APP_MENU)
                    .build();

    /** Tests that "New tab" opens a new tab with the NTP. */
    @Test
    @LargeTest
    public void testOpenNewTab() {
        mCtaTestRule.startOnBlankPage().openRegularTabAppMenu().openNewTab();

        assertEquals(2, mCtaTestRule.tabsCount(/* incognito= */ false));
        assertEquals(0, mCtaTestRule.tabsCount(/* incognito= */ true));
    }

    /**
     * Tests that "New Incognito tab/window" opens a new incognito tab/window with the incognito
     * NTP.
     */
    @Test
    @LargeTest
    public void testOpenNewIncognitoTabOrWindow() {
        // openNewIncognitoTab() opens either an incognito tab or an incognito window.
        var incognitoNewTabPageStation =
                mCtaTestRule.startOnBlankPage().openRegularTabAppMenu().openNewIncognitoTab();

        // TabModel#getCount() requires the UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(1, incognitoNewTabPageStation.getTabModel().getCount()));
    }

    /** Tests that "Bookmarks" opens the Bookmarks page. */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testOpenBookmarksTablet() {
        WebPageStation pageStation = mCtaTestRule.startOnBlankPage();

        pageStation.openRegularTabAppMenu().openBookmarksTablet();
    }

    /** Tests that "Bookmarks" opens the Bookmarks page. */
    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOpenBookmarksPhone() {
        WebPageStation pageStation = mCtaTestRule.startOnBlankPage();

        BookmarksPhoneStation bookmarks = pageStation.openRegularTabAppMenu().openBookmarksPhone();

        // Exit bookmarks for the initial state rule to be able to reset state.
        bookmarks.pressBackTo().arriveAt(WebPageStation.newBuilder().initFrom(pageStation).build());
    }

    /** Tests that "Settings" opens the SettingsActivity. */
    @Test
    @LargeTest
    public void testOpenSettings() {
        WebPageStation pageStation = mCtaTestRule.startOnBlankPage();
        Tab tab = pageStation.loadedTabElement.value();
        SettingsStation settings = pageStation.openRegularTabAppMenu().openSettings();

        // Exit settings for the initial state rule to be able to reset state.
        settings.pressBackTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .withIncognito(false)
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

        String appMenuGoldenId =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? "regular_ntp_app_menu_with_open_incognito_window"
                        : "regular_ntp_app_menu_with_open_incognito_tab";
        mRenderTestRule.render(menu.menuListElement.value(), appMenuGoldenId);
        menu.verifyPresentItems();

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

        String appMenuGoldenId =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? "incognito_ntp_app_menu_with_open_incognito_window"
                        : "incognito_ntp_app_menu_with_open_incognito_tab";
        mRenderTestRule.render(menu.menuListElement.value(), appMenuGoldenId);
        menu.verifyPresentItems();

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

        String appMenuGoldenId =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? "regular_webpage_app_menu_with_open_incognito_window"
                        : "regular_webpage_app_menu_with_open_incognito_tab";
        mRenderTestRule.render(menu.menuListElement.value(), appMenuGoldenId);
        menu.verifyPresentItems();

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

        String appMenuGoldenId =
                IncognitoUtils.shouldOpenIncognitoAsWindow()
                        ? "incognito_webpage_app_menu_with_open_incognito_window"
                        : "incognito_webpage_app_menu_with_open_incognito_tab";
        mRenderTestRule.render(menu.menuListElement.value(), appMenuGoldenId);
        menu.verifyPresentItems();

        // Clean up for next tests in batch
        menu.clickOutsideToClose();
    }

    /** Tests that entering the Tab Switcher causes the app menu to close. */
    @Test
    @LargeTest
    public void testHideMenuOnToggleOverview() {
        WebPageStation page = mCtaTestRule.startOnBlankPage();

        page.openRegularTabAppMenu();

        // Go to Tab Switcher programmatically because the App Menu covers the button.
        RegularTabSwitcherStation tabSwitcher =
                RegularTabSwitcherStation.from(page.getTabModelSelector());
        ChromeTriggers.showTabSwitcherLayoutTo(page).arriveAt(tabSwitcher);

        tabSwitcher.openAppMenu();

        // Go to a Web Page programmatically because tapping outside the app menu causes it to
        // capture the event and close.
        ChromeTriggers.showBrowsingLayoutTo(tabSwitcher)
                .arriveAt(WebPageStation.newBuilder().initFrom(page).build());
    }
}
