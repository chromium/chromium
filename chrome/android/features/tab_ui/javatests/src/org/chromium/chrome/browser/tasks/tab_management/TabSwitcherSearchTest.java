// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.DeviceFormFactor.PHONE;

import androidx.core.util.Pair;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.NewTabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherListEditorFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation.SuggestionFacility;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for search in the tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/419289558): Re-enable color surface feature flags.
@DisableFeatures({
    ChromeFeatureList.ANDROID_THEME_MODULE,
    OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS
})
@Batch(Batch.PER_CLASS)
public class TabSwitcherSearchTest {
    private static final int SERVER_PORT = 13245;
    private static final String URL_PREFIX = "127.0.0.1:" + SERVER_PORT;

    // The Activity doesn't get reused because tearDown() closes it, but resetting the tab state
    // is necessary for some tests.
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mPage;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mTestServer =
                mCtaTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT).getServer();
        mPage = mCtaTestRule.startOnBlankPage();
        mUserActionTester = new UserActionTester();

        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ActivityFinisher.finishAll();
    }

    @Test
    @MediumTest
    @Restriction(PHONE)
    public void testHubSearchBox_Phone() {
        RegularTabSwitcherStation tabSwitcher = mPage.openRegularTabSwitcher();
        assertEquals(R.id.search_box, tabSwitcher.searchElement.value().getId());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testHubSearchLoupe_Tablet() {
        RegularTabSwitcherStation tabSwitcher = mPage.openRegularTabSwitcher();
        assertEquals(R.id.search_loupe, tabSwitcher.searchElement.value().getId());
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions() {
        List<String> urlsToOpen =
                List.of(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();

        // ZPS for open tabs only shows the most recent 4 tabs.
        tabSwitcherSearchStation.findSuggestionsByText(urlsToOpen, URL_PREFIX);
        // Check the header text.
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Last open tabs");
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSuggestion() {
        List<String> urlsToOpen =
                List.of(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();

        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ null,
                        /* title= */ null,
                        /* text= */ URL_PREFIX + urlsToOpen.get(0));
        mPage = suggestion.openPage();
        assertEquals(
                mTestServer.getURL(urlsToOpen.get(0)),
                mPage.loadedTabElement.value().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSameTab() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        Tab initialTab = mPage.loadedTabElement.value();
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();

        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ null,
                        /* title= */ null,
                        /* text= */ URL_PREFIX + urlsToOpen.get(0));
        mPage = suggestion.openPage();
        assertSame(initialTab, mPage.loadedTabElement.value());
    }

    @Test
    @MediumTest
    // Regression test for the currently selected tab being included/excluded randomly.
    public void testZeroPrefixSuggestions_IgnoresHiddenTabs() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        List<String> urlsToOpen =
                List.of(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();

        // ZPS for open tabs only shows the most recent 4 tabs.
        tabSwitcherSearchStation.findSuggestionsByText(urlsToOpen, URL_PREFIX);

        // Check the header text.
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Last open tabs");

        RegularTabSwitcherStation tabSwitcher =
                tabSwitcherSearchStation.pressBackToRegularTabSwitcher(cta);
        tabSwitcherSearchStation = tabSwitcher.openTabSwitcherSearch();

        // ZPS for open tabs only shows the most recent 4 tabs.
        tabSwitcherSearchStation.findSuggestionsByText(urlsToOpen, URL_PREFIX);

        // Check the header text.
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Last open tabs");
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_Incognito() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openIncognitoTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsNotShown();
    }

    @Test
    @LargeTest
    public void testZeroPrefixSuggestions_duplicateUrls() {
        List<String> urlsToOpen =
                List.of(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/test.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();

        // Tab URLs will be de-duped.
        tabSwitcherSearchStation.findSuggestionsByText(urlsToOpen, URL_PREFIX);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.checkSuggestionsShown();
        tabSwitcherSearchStation.findSuggestionsByText(urlsToOpen, URL_PREFIX);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSuggestion() {
        List<String> urlsToOpen =
                List.of(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/test.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("test.html");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 0,
                        /* title= */ URL_PREFIX + "/chrome/test/data/android/test.html",
                        /* text= */ URL_PREFIX + "/chrome/test/data/android/test.html");
        suggestion.openPage();
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSameTab() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        Tab initialTab = mPage.loadedTabElement.value();
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 0,
                        /* title= */ "One",
                        /* text= */ URL_PREFIX + urlsToOpen.get(0));
        mPage = suggestion.openPage();
        assertEquals("One", mPage.loadedTabElement.value().getTitle());
        assertSame(initialTab, mPage.loadedTabElement.value());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSuggestionWithEnter() {
        List<String> urlsToOpen =
                List.of(
                        "/chrome/test/data/android/navigate/one.html",
                        "/chrome/test/data/android/test.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 0, /* title= */ "One", /* text= */ null);
        mPage = suggestion.openPagePressingEnter();
        assertEquals("One", mPage.loadedTabElement.value().getTitle());
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_Incognito() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openIncognitoTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.findSuggestion(
                /* index= */ 0, /* title= */ "One", /* text= */ URL_PREFIX + urlsToOpen.get(0));
    }

    @Test
    @MediumTest
    public void testSearchActivityBackButton() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        mPage.openRegularTabSwitcher().openTabSwitcherSearch().pressBackToRegularTabSwitcher(cta);
    }

    @Test
    @MediumTest
    public void testSearchActivityBackButton_Incognito() {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        mPage.openNewIncognitoTabOrWindowFast()
                .openIncognitoTabSwitcher()
                .openTabSwitcherSearch()
                .pressBackToIncognitoTabSwitcher(cta);
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSearchSuggestion() {
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("foobar");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Search the web");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 1, /* title= */ "foobar", /* text= */ null);
        mPage = suggestion.openPage();
        assertFalse(mPage.isIncognito());
    }

    @Test
    @MediumTest
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    public void testTypedSuggestions_OpenTabGroupSearchSuggestion() {
        String tabGroupTitle = "Test";
        Tab firstTab = mPage.loadedTabElement.value();
        int firstTabId = firstTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                firstTab);
        RegularNewTabPageStation secondPage = mPage.openNewTabFast();
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
        dialog = dialog.inputName(tabGroupTitle);
        dialog.pressDone();

        TabSwitcherSearchStation tabSwitcherSearchStation = tabSwitcher.openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("test");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Tabs and tab groups");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 2,
                        /* title= */ "   Test",
                        /* text= */ "127.0.0.1:13245/chrome/test/data/android/navigate/one.html,"
                                + " 127.0.0.1:13245/chrome/test/data/android/navigate/one.html");
        Pair<RegularTabSwitcherStation, TabGroupDialogFacility> pair =
                suggestion.openTabGroup(
                        mCtaTestRule.getActivity(),
                        List.of(firstTabId, secondTabId),
                        tabGroupTitle);
        assertEquals(tabGroupTitle, pair.second.getTitle());
        assertEquals(
                1,
                mUserActionTester.getActionCount("TabGroups.HubSearchTabGroupSuggestionClicked"));
    }

    @Test
    @MediumTest
    @EnableFeatures({OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS})
    public void testTypedSuggestions_OpenTabGroupSearchSuggestionByURLMatch() {
        String tabGroupTitle = "Test";
        Tab firstTab = mPage.loadedTabElement.value();
        int firstTabId = firstTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                firstTab);
        RegularNewTabPageStation secondPage = mPage.openNewTabFast();
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
        dialog = dialog.inputName(tabGroupTitle);
        dialog.pressDone();

        TabSwitcherSearchStation tabSwitcherSearchStation = tabSwitcher.openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("navigate");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Tabs and tab groups");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 2,
                        /* title= */ "   Test",
                        /* text= */ "127.0.0.1:13245/chrome/test/data/android/navigate/one.html,"
                                + " 127.0.0.1:13245/chrome/test/data/android/navigate/one.html");
        Pair<RegularTabSwitcherStation, TabGroupDialogFacility> pair =
                suggestion.openTabGroup(
                        mCtaTestRule.getActivity(),
                        List.of(firstTabId, secondTabId),
                        tabGroupTitle);
        assertEquals(tabGroupTitle, pair.second.getTitle());
        assertEquals(
                1,
                mUserActionTester.getActionCount("TabGroups.HubSearchTabGroupSuggestionClicked"));
    }

    @Test
    @MediumTest
    @EnableFeatures({
        OmniboxFeatureList.ANDROID_HUB_SEARCH_TAB_GROUPS + ":enable_hub_search_tab_groups_pane/true"
    })
    public void testTypedSuggestionsFromTabGroupsPane_OpenTabGroupSearchSuggestion() {
        String tabGroupTitle = "Test";
        Tab firstTab = mPage.loadedTabElement.value();
        int firstTabId = firstTab.getId();
        mCtaTestRule.loadUrlInTab(
                mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_ONE),
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                firstTab);
        RegularNewTabPageStation secondPage = mPage.openNewTabFast();
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
        dialog = dialog.inputName(tabGroupTitle);
        dialog.pressDone();

        TabSwitcherSearchStation tabSwitcherSearchStation =
                tabSwitcher.selectTabGroupsPane().openTabGroupsPaneSearch();
        tabSwitcherSearchStation.typeInOmnibox("test");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Tabs and tab groups");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 2,
                        /* title= */ "   Test",
                        /* text= */ "127.0.0.1:13245/chrome/test/data/android/navigate/one.html,"
                                + " 127.0.0.1:13245/chrome/test/data/android/navigate/one.html");
        Pair<RegularTabSwitcherStation, TabGroupDialogFacility> pair =
                suggestion.openTabGroup(
                        mCtaTestRule.getActivity(),
                        List.of(firstTabId, secondTabId),
                        tabGroupTitle);
        assertEquals(tabGroupTitle, pair.second.getTitle());
        assertEquals(
                1,
                mUserActionTester.getActionCount("TabGroups.HubSearchTabGroupSuggestionClicked"));
    }

    @Test
    @MediumTest
    public void testTypedSuggestions_OpenSearchSuggestion_Incognito() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.createIncognitoTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openIncognitoTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("foobar");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Search the web");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 1, /* title= */ "foobar", /* text= */ null);
        mPage = suggestion.openPage();
        assertTrue(mPage.isIncognito());
    }

    @Test
    @MediumTest
    @RequiresRestart("Adding the bookmark affects suggestions in subsequent tests")
    // TODO(crbug.com/394401323): Add some PT station for searching bookmarks.
    public void testBookmarkSuggestions() {
        WebPageStation openPage =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"));
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mCtaTestRule.getActivity(),
                R.id.bookmark_this_page_id);

        TabSwitcherSearchStation tabSwitcherSearchStation =
                openPage.loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/test.html"))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "Bookmarks");
        tabSwitcherSearchStation.findSuggestion(
                /* index= */ 1, /* title= */ "One", /* text= */ null);
    }

    @Test
    @MediumTest
    // TODO(crbug.com/394401463): Add some PT station for searching history.
    public void testHistorySuggestions() throws TimeoutException {
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openNewTabFast()
                        .loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/navigate/one.html"))
                        .loadWebPageProgrammatically(
                                mTestServer.getURL("/chrome/test/data/android/test.html"))
                        .openRegularTabSwitcher()
                        .openTabSwitcherSearch();

        CallbackHelper helper = new CallbackHelper();
        BrowsingHistoryBridge historyBridge =
                runOnUiThreadBlocking(
                        () ->
                                new BrowsingHistoryBridge(
                                        mCtaTestRule
                                                .getActivity()
                                                .getProfileProviderSupplier()
                                                .get()
                                                .getOriginalProfile()));
        historyBridge.setObserver(
                new BrowsingHistoryObserver() {
                    @Override
                    public void onQueryHistoryComplete(
                            List<HistoryItem> items, boolean hasMorePotentialMatches) {
                        if (items.size() > 0) {
                            for (HistoryItem item : items) {
                                if (item.getTitle().contains("One")) {
                                    helper.notifyCalled();
                                }
                            }
                        }
                    }

                    @Override
                    public void onHistoryDeleted() {}

                    @Override
                    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {}

                    @Override
                    public void onQueryAppsComplete(List<String> items) {}
                });
        runOnUiThreadBlocking(() -> historyBridge.queryHistory("one.html", /* appId= */ null));
        helper.waitForNext();

        tabSwitcherSearchStation.typeInOmnibox("One");
        tabSwitcherSearchStation.findSectionHeaderByIndexAndText(0, "History");
        tabSwitcherSearchStation.findSuggestion(
                /* index= */ 1, /* title= */ "One", /* text= */ null);
    }
}
