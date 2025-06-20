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
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
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
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation;
import org.chromium.chrome.test.transit.hub.TabSwitcherSearchStation.SuggestionFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for search in the tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
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

    @Before
    public void setUp() {
        mTestServer =
                TabSwitcherSearchTestUtils.setServerPortAndGetTestServer(
                        mCtaTestRule.getActivityTestRule(), SERVER_PORT);
        mPage = mCtaTestRule.startOnBlankPage();

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
        assertEquals(R.id.search_box, tabSwitcher.searchElement.get().getId());
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testHubSearchLoupe_Tablet() {
        RegularTabSwitcherStation tabSwitcher = mPage.openRegularTabSwitcher();
        assertEquals(R.id.search_loupe, tabSwitcher.searchElement.get().getId());
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
                mPage.loadedTabElement.get().getUrl().getSpec());
    }

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_OpenSameTab() {
        List<String> urlsToOpen = List.of("/chrome/test/data/android/navigate/one.html");
        mPage = Journeys.prepareRegularTabsWithWebPages(mPage, mTestServer.getURLs(urlsToOpen));
        Tab initialTab = mPage.loadedTabElement.get();
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.checkSuggestionsShown();

        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ null,
                        /* title= */ null,
                        /* text= */ URL_PREFIX + urlsToOpen.get(0));
        mPage = suggestion.openPage();
        assertSame(initialTab, mPage.loadedTabElement.get());
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
        Tab initialTab = mPage.loadedTabElement.get();
        TabSwitcherSearchStation tabSwitcherSearchStation =
                mPage.openRegularTabSwitcher().openTabSwitcherSearch();
        tabSwitcherSearchStation.typeInOmnibox("one.html");
        SuggestionFacility suggestion =
                tabSwitcherSearchStation.findSuggestion(
                        /* index= */ 0,
                        /* title= */ "One",
                        /* text= */ URL_PREFIX + urlsToOpen.get(0));
        mPage = suggestion.openPage();
        assertEquals("One", mPage.loadedTabElement.get().getTitle());
        assertSame(initialTab, mPage.loadedTabElement.get());
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
        assertEquals("One", mPage.loadedTabElement.get().getTitle());
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
        mPage.openNewIncognitoTabFast()
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
