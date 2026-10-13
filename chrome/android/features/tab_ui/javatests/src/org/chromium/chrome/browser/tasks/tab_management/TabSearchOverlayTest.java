// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
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
import org.chromium.chrome.test.util.VerticalTabsTestUtils;
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
