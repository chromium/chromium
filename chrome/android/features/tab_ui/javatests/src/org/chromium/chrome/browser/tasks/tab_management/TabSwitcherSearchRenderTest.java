// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static org.chromium.ui.base.DeviceFormFactor.PHONE;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for search in the tab switcher. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/419289558): Re-enable color surface feature flags.
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE,
    ChromeFeatureList.ANDROID_THEME_MODULE,
    ChromeFeatureList.HISTORY_PANE_ANDROID,
    OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS
})
public class TabSwitcherSearchRenderTest {
    private static final int SERVER_PORT = 13245;

    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(14)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_SWITCHER)
                    .build();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mInitialPage;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled));
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Before
    public void setUp() throws ExecutionException {
        mTestServer =
                mCtaTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT).getServer();
        mInitialPage = mCtaTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        ActivityFinisher.finishAll();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(PHONE)
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    public void testHubSearchBox_Phone() throws IOException {
        OmniboxFeatures.sAndroidHubSearchEnableTabGroupStrings.setForTesting(true);

        RegularTabSwitcherStation tabSwitcher = mInitialPage.openRegularTabSwitcher();

        mRenderTestRule.render(tabSwitcher.viewHolderElement.value(), "hub_searchbox_phone");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(PHONE)
    @DisabledTest(message = "https://crbug.com/426664421")
    public void testHubSearchBox_Phone_Incognito() throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        IncognitoTabSwitcherStation tabSwitcher =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher();

        mRenderTestRule.render(
                tabSwitcher.viewHolderElement.value(), "hub_searchbox_phone_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(PHONE)
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    public void testHubSearchBox_PhoneLandscape() throws IOException {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_LANDSCAPE);
        RegularTabSwitcherStation tabSwitcher = mInitialPage.openRegularTabSwitcher();

        mRenderTestRule.render(
                tabSwitcher.viewHolderElement.value(), "hub_searchbox_phone_landscape");
        ActivityTestUtils.clearActivityOrientation(cta);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testHubSearchLoupe_Tablet(boolean nightModeEnabled) throws IOException {
        RegularTabSwitcherStation tabSwitcher = mInitialPage.openRegularTabSwitcher();

        mRenderTestRule.render(tabSwitcher.viewHolderElement.value(), "hub_searchloupe_tablet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    // TODO(crbug.com/439491767): Fix broken tests caused by desktop-like incognito window.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testHubSearchLoupe_Tablet_Incognito() throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        IncognitoTabSwitcherStation tabSwitcher =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher();

        mRenderTestRule.render(
                tabSwitcher.viewHolderElement.value(), "hub_searchloupe_tablet_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testZeroPrefixSuggestions_ShownInRegular(boolean nightModeEnabled)
            throws IOException {
        OmniboxFeatures.sAndroidHubSearchEnableTabGroupStrings.setForTesting(true);

        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/test.html");
        TabSwitcherSearchStation searchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        searchStation.checkSuggestionsShown();

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content), "hub_search_zps");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testZeroPrefixSuggestions_HiddenInIncognito() throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/test.html");
        TabSwitcherSearchStation searchStation =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher()
                        .openTabSwitcherSearch();
        searchStation.checkSuggestionsNotShown();

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content),
                "hub_search_zps_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderTypedSuggestions(boolean nightModeEnabled) throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation searchStation =
                Journeys.prepareRegularTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        searchStation.typeInOmnibox("one.html");
        searchStation.findSuggestion(/* index= */ 0, /* title= */ "One", /* text= */ null);

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content), "hub_search_typed");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderTypedSuggestions_Incognito() throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchStation searchStation =
                Journeys.createIncognitoTabsWithWebPages(
                                mInitialPage, mTestServer.getURLs(urlsToOpen))
                        .openIncognitoTabSwitcher()
                        .openTabSwitcherSearch();
        searchStation.typeInOmnibox("one.html");
        searchStation.findSuggestion(/* index= */ 0, /* title= */ "One", /* text= */ null);

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content),
                "hub_search_typed_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderTypedTabGroupSuggestions(boolean nightModeEnabled) throws IOException {
        Tab firstTab = mInitialPage.loadedTabElement.value();
        int firstTabId = firstTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                firstTab);
        RegularNewTabPageStation secondPage = mInitialPage.openNewTabFast();
        Tab secondTab = secondPage.loadedTabElement.value();
        int secondTabId = secondTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                secondTab);
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility<RegularTabSwitcherStation> editor =
                tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);
        NewTabGroupDialogFacility<RegularTabSwitcherStation> dialog =
                editor.openAppMenuWithEditor().groupTabs();
        dialog = dialog.inputName("Foobar");
        dialog = dialog.pickColor(TabGroupColorId.BLUE);
        dialog.pressDone();

        TabSwitcherSearchStation searchStation = tabSwitcher.openTabSwitcherSearch();
        searchStation.typeInOmnibox("foo");
        searchStation.findSectionHeaderByIndexAndText(0, "Tabs and tab groups");
        searchStation.findSuggestion(
                /* index= */ 1,
                /* title= */ "   Foobar",
                /* text= */ "127.0.0.1:13245/chrome/test/data/android/navigate/one.html,"
                        + " 127.0.0.1:13245/chrome/test/data/android/navigate/one.html");

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content),
                "hub_search_typed_tab_group");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderTypedTabGroupSuggestions_URLMatch(boolean nightModeEnabled)
            throws IOException {
        Tab firstTab = mInitialPage.loadedTabElement.value();
        int firstTabId = firstTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                firstTab);
        RegularNewTabPageStation secondPage = mInitialPage.openNewTabFast();
        Tab secondTab = secondPage.loadedTabElement.value();
        int secondTabId = secondTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                secondTab);
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility<RegularTabSwitcherStation> editor =
                tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);
        NewTabGroupDialogFacility<RegularTabSwitcherStation> dialog =
                editor.openAppMenuWithEditor().groupTabs();
        dialog = dialog.inputName("Test");
        dialog = dialog.pickColor(TabGroupColorId.BLUE);
        dialog.pressDone();

        TabSwitcherSearchStation searchStation = tabSwitcher.openTabSwitcherSearch();
        searchStation.typeInOmnibox("data");
        searchStation.findSectionHeaderByIndexAndText(0, "Tabs and tab groups");
        searchStation.findSuggestion(
                /* index= */ 2,
                /* title= */ "   Test",
                /* text= */ "127.0.0.1:13245/chrome/test/data/android/navigate/one.html,"
                        + " 127.0.0.1:13245/chrome/test/data/android/navigate/one.html");

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content),
                "hub_search_typed_tab_group_url_match");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderTypedTabGroupSuggestions_ChromePrefixedTabsOmmitted(
            boolean nightModeEnabled) throws IOException {
        int firstTabId = mInitialPage.loadedTabElement.value().getId();
        RegularNewTabPageStation secondPage = mInitialPage.openNewTabFast();
        int secondTabId = secondPage.loadedTabElement.value().getId();
        RegularTabSwitcherStation tabSwitcher = secondPage.openRegularTabSwitcher();
        TabSwitcherListEditorFacility<RegularTabSwitcherStation> editor =
                tabSwitcher.openAppMenu().clickSelectTabs();
        editor = editor.addTabToSelection(0, firstTabId);
        editor = editor.addTabToSelection(1, secondTabId);
        NewTabGroupDialogFacility<RegularTabSwitcherStation> dialog =
                editor.openAppMenuWithEditor().groupTabs();
        dialog = dialog.inputName("Test");
        dialog = dialog.pickColor(TabGroupColorId.BLUE);
        dialog.pressDone();

        TabSwitcherSearchStation searchStation = tabSwitcher.openTabSwitcherSearch();
        searchStation.typeInOmnibox("test");
        searchStation.findSectionHeaderByIndexAndText(0, "Tabs and tab groups");
        searchStation.findSuggestion(/* index= */ 1, /* title= */ "   Test", /* text= */ null);

        mRenderTestRule.render(
                searchStation.getActivity().findViewById(android.R.id.content),
                "hub_search_typed_tab_group_match_no_urls");
    }
}
