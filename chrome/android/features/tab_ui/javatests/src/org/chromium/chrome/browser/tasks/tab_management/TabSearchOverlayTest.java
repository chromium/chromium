// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider.BrowsingHistoryObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabSearchOverlayCoordinator.TabSearchDismissalReason;
import org.chromium.chrome.browser.tasks.tab_management.TabSearchOverlayCoordinator.TabSearchEntryPoint;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility.TabGroupSuggestionFacility;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility.TabSuggestionFacility;
import org.chromium.chrome.test.util.TabRestoreServiceUtils;
import org.chromium.chrome.test.util.VerticalTabsTestUtils;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for the Tab Search Overlay using Public Transit. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP, ChromeFeatureList.ANDROID_VERTICAL_TABS})
@Restriction({DeviceFormFactor.DESKTOP})
@Batch(Batch.PER_CLASS)
public class TabSearchOverlayTest {
    private static final int SERVER_PORT = 13245;

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mTestServer =
                mCtaTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT).getServer();
        // Clear recently closed entries left by earlier tests in the batch, including tabs closed
        // by the auto-reset that runs before each test.
        TabRestoreServiceUtils.clearEntries(mCtaTestRule.getActivity().getTabModelSelector());
    }

    // --- Entry Point Tests ---

    @Test
    @MediumTest
    public void testEntryPoint_HorizontalTabStrip() {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        mPage = mCtaTestRule.startOnWebPage(url);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.EntryPoint", TabSearchEntryPoint.HORIZONTAL_TAB_STRIP);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        watcher.assertExpected();

        tabSearchOverlay.dismissViaCloseButton();
    }

    @Test
    @MediumTest
    public void testEntryPoint_VerticalTabs() {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        mPage = mCtaTestRule.startOnWebPage(url);

        try (var ignored =
                VerticalTabsTestUtils.toggleTabStripForTesting(mCtaTestRule.getActivity())) {
            HistogramWatcher watcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            "Android.TabSearch.EntryPoint", TabSearchEntryPoint.VERTICAL_TABS);

            TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay =
                    mPage.openTabSearchOverlay();
            watcher.assertExpected();

            tabSearchOverlay.dismissViaCloseButton();
        }
    }

    @Test
    @MediumTest
    public void testEntryPoint_KeyboardShortcut() {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        mPage = mCtaTestRule.startOnWebPage(url);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.EntryPoint", TabSearchEntryPoint.KEYBOARD_SHORTCUT);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay =
                mPage.openTabSearchOverlayViaShortcut();
        watcher.assertExpected();

        tabSearchOverlay.dismissViaCloseButton();
    }

    // --- Dismissal Method Tests ---

    @Test
    @MediumTest
    public void testDismissOverlay_CloseButton() {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        mPage = mCtaTestRule.startOnWebPage(url);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.CLOSE_BUTTON);

        tabSearchOverlay.dismissViaCloseButton();

        watcher.assertExpected();
        assertOverlayNotVisible();
    }

    @Test
    @MediumTest
    public void testDismissOverlay_ScrimTouch() {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        mPage = mCtaTestRule.startOnWebPage(url);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.SCRIM);

        tabSearchOverlay.dismissViaScrim();

        watcher.assertExpected();
        assertOverlayNotVisible();
    }

    @Test
    @MediumTest
    public void testDismissOverlay_BackPress() {
        String url = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        mPage = mCtaTestRule.startOnWebPage(url);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.BACK_PRESS);

        tabSearchOverlay.dismissViaBack();

        watcher.assertExpected();
        assertOverlayNotVisible();
    }

    // --- Open Tab Selection Tests ---

    @Test
    @MediumTest
    public void testOpenExistingTab_ReusesTabWithoutIncreasingCount() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/navigate/two.html");

        mPage = mCtaTestRule.startOnWebPage(url1);
        Tab tab1 = mPage.getTab();

        mPage = mPage.openFakeLinkToWebPage(url2);
        Tab tab2 = mPage.getTab();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.TAB_SELECTED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.typeInSearchBox("one.html");

        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ null, /* title= */ "One", /* urlSubstring= */ null);
        mPage = suggestion.clickToSelectTab();

        // Verify the existing tab was reused and tab count remained 2.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel normalTabModel =
                            cta.getTabModelSelector().getModel(/* incognito= */ false);
                    assertEquals(2, normalTabModel.getCount());
                    assertEquals(tab1, mPage.getTab());
                    assertEquals("One", mPage.getTab().getTitle());
                });
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testOpenExistingTab_PressEnter_ReusesTab() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/navigate/two.html");

        mPage = mCtaTestRule.startOnWebPage(url1);
        Tab tab1 = mPage.getTab();

        mPage = mPage.openFakeLinkToWebPage(url2);
        Tab tab2 = mPage.getTab();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.URL_LOADED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.typeInSearchBox("one.html");

        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ 0, /* title= */ "One", /* urlSubstring= */ null);
        mPage = suggestion.pressEnterToSelectTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel normalTabModel =
                            cta.getTabModelSelector().getModel(/* incognito= */ false);
                    assertEquals(2, normalTabModel.getCount());
                    assertEquals(tab1, mPage.getTab());
                    assertEquals("One", mPage.getTab().getTitle());
                });
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    // --- Zero-Prefix Suggestions (ZPS) Tests ---

    @Test
    @MediumTest
    public void testZeroPrefixSuggestions_ClickOpenTab() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/navigate/two.html");

        mPage = mCtaTestRule.startOnWebPage(url1);
        Tab tab1 = mPage.getTab();

        mPage = mPage.openFakeLinkToWebPage(url2);
        Tab tab2 = mPage.getTab();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.TAB_SELECTED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.checkSuggestionsShown();

        // In ZPS, open tabs are immediately presented without typing.
        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ null, /* title= */ "One", /* urlSubstring= */ null);
        mPage = suggestion.clickToSelectTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel normalTabModel =
                            cta.getTabModelSelector().getModel(/* incognito= */ false);
                    assertEquals(2, normalTabModel.getCount());
                    assertEquals(tab1, mPage.getTab());
                    assertEquals("One", mPage.getTab().getTitle());
                });
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    // --- History Suggestions Tests ---

    @Test
    @MediumTest
    public void testSelectHistorySuggestion_OpensInNewTab() throws TimeoutException {
        String urlHistory = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        String urlCurrent = mTestServer.getURL("/chrome/test/data/android/test.html");

        // Start on urlHistory, then navigate to urlCurrent so urlHistory is now only in history.
        mPage = mCtaTestRule.startOnWebPage(urlHistory);
        mPage = mPage.loadWebPageProgrammatically(urlCurrent);

        // Ensure BrowsingHistoryBridge has committed the history entry.
        CallbackHelper helper = new CallbackHelper();
        BrowsingHistoryBridge historyBridge =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                new BrowsingHistoryBridge(
                                        mCtaTestRule
                                                .getActivity()
                                                .getProfileProviderSupplier()
                                                .get()
                                                .getOriginalProfile()));
        try {
            historyBridge.setObserver(
                    new BrowsingHistoryObserver() {
                        @Override
                        public void onQueryHistoryComplete(
                                List<HistoryItem> items, boolean hasMorePotentialMatches) {
                            if (items != null) {
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
            ThreadUtils.runOnUiThreadBlocking(() -> historyBridge.queryHistory("one.html"));
            helper.waitForNext();
        } finally {
            ThreadUtils.runOnUiThreadBlocking(historyBridge::destroy);
        }

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.URL_LOADED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.typeInSearchBox("One");

        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ null, /* title= */ "One", /* urlSubstring= */ null);
        mPage = suggestion.clickToOpenNewTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel normalTabModel =
                            cta.getTabModelSelector().getModel(/* incognito= */ false);
                    // History suggestions open into a new tab, increasing the count from 1 to 2.
                    assertEquals(2, normalTabModel.getCount());
                    assertEquals("One", mPage.getTab().getTitle());
                });
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    // --- Tab Group Suggestions Tests ---

    @Test
    @MediumTest
    public void testSelectTabGroupSuggestion_SwitchesToTabGroup() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/navigate/two.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/navigate/three.html");

        mPage = mCtaTestRule.startOnWebPage(url1);
        Tab tab1 = mPage.getTab();

        mPage = mPage.openFakeLinkToWebPage(url2);
        Tab tab2 = mPage.getTab();

        mPage = mPage.openFakeLinkToWebPage(url3);
        Tab tab3 = mPage.getTab();

        final String tabGroupTitle = "ResearchGroup";

        // Group tab1 and tab2 into a tab group titled "ResearchGroup".
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel tabModel = cta.getTabModelSelector().getModel(/* incognito= */ false);
                    tabModel.mergeTabsToGroup(tab2.getId(), tab1.getId());
                    tabModel.setTabGroupTitle(tab1.getTabGroupId(), tabGroupTitle);
                });

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason",
                        TabSearchDismissalReason.TAB_GROUP_SELECTED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.typeInSearchBox(tabGroupTitle);

        TabGroupSuggestionFacility suggestion =
                tabSearchOverlay.findTabGroupSuggestion(
                        /* index= */ null, /* groupTitle= */ tabGroupTitle);
        mPage = suggestion.clickToSelectTabGroup();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel normalTabModel =
                            cta.getTabModelSelector().getModel(/* incognito= */ false);
                    assertEquals(3, normalTabModel.getCount());
                    // The selected tab should now be one of the tabs in the group.
                    Tab currentTab = normalTabModel.getTabAt(normalTabModel.index());
                    assertEquals(tab1.getTabGroupId(), currentTab.getTabGroupId());
                });
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    // --- Recently Closed Tab Tests ---

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_TAB_SEARCH_RECENT_TABS)
    public void testZeroPrefixSuggestions_ClickRecentlyClosedTab_RestoresTab() {
        closeTabWithHistory();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.TAB_RESTORED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.checkSuggestionsShown();

        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ null, /* title= */ "Two", /* urlSubstring= */ null);
        mPage = suggestion.clickToOpenNewTab();

        assertClosedTabRestored();
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_TAB_SEARCH_RECENT_TABS)
    public void testRecentlyClosedTab_PressEnter_RestoresTab() {
        closeTabWithHistory();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.TAB_RESTORED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        // Search by title: a URL-prefix query would let the history match for the same URL win
        // deduplication (crbug.com/575623966).
        tabSearchOverlay.typeInSearchBox("Two");

        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ null, /* title= */ "Two", /* urlSubstring= */ null);
        mPage = suggestion.pressEnterToOpenNewTab();

        assertClosedTabRestored();
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(OmniboxFeatureList.ANDROID_TAB_SEARCH_RECENT_TABS)
    @DisabledTest(message = "crbug.com/575623966")
    public void testRecentlyClosedTab_UrlPrefixQuery_RestoresTab() {
        closeTabWithHistory();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.TAB_RESTORED);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay = mPage.openTabSearchOverlay();
        // A URL-prefix query lets the history match for the same URL compete in deduplication.
        tabSearchOverlay.typeInSearchBox("127.0");

        TabSuggestionFacility suggestion =
                tabSearchOverlay.findTabSuggestion(
                        /* index= */ null, /* title= */ "Two", /* urlSubstring= */ null);
        mPage = suggestion.clickToOpenNewTab();

        assertClosedTabRestored();
        assertOverlayNotVisible();
        watcher.assertExpected();
    }

    /**
     * Closes the first tab after navigating it from "One" to "Two", leaving a second tab on "Three"
     * as the current page.
     */
    private void closeTabWithHistory() {
        mPage =
                mCtaTestRule.startOnWebPage(
                        mTestServer.getURL("/chrome/test/data/android/navigate/one.html"));
        mPage =
                mPage.loadWebPageProgrammatically(
                        mTestServer.getURL("/chrome/test/data/android/navigate/two.html"));
        Tab tabToClose = mPage.getTab();
        mPage =
                mPage.openFakeLinkToWebPage(
                        mTestServer.getURL("/chrome/test/data/android/navigate/three.html"));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        getNormalTabModel()
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tabToClose)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));
    }

    /** Asserts the tab closed by {@link #closeTabWithHistory()} is restored and selected. */
    private void assertClosedTabRestored() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel normalTabModel = getNormalTabModel();
                    Tab restoredTab = mPage.getTab();
                    assertEquals(2, normalTabModel.getCount());
                    assertEquals("Two", restoredTab.getTitle());
                    // The tab's original position and navigation history are restored.
                    assertEquals(0, normalTabModel.indexOf(restoredTab));
                    assertTrue(restoredTab.canGoBack());
                });
    }

    private TabModel getNormalTabModel() {
        return mCtaTestRule.getActivity().getTabModelSelector().getModel(/* incognito= */ false);
    }

    private void assertOverlayNotVisible() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator rootUiCoordinator =
                            (TabbedRootUiCoordinator)
                                    mCtaTestRule.getActivity().getRootUiCoordinatorForTesting();
                    assertFalse(
                            rootUiCoordinator
                                    .getTabSearchOverlayCoordinatorForTesting()
                                    .isVisible());
                });
    }
}
