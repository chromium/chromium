// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.tab_restore;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.RecentlyClosedBulkEvent;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedGroup;
import org.chromium.chrome.browser.ntp.RecentlyClosedTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabRestoreServiceUtils;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * End to end tests for {@link HistoricalTabSaverImpl} and its interactions with TabRestoreService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
@DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
public class HistoricalTabSaverImplTest {
    private static final String TEST_PAGE_1 = "/chrome/test/data/android/about.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/simple.html";
    private static final String TEST_PAGE_3 = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_4 = "/chrome/test/data/android/theme_color_test.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;
    private TabModelSelector mTabModelSelector;
    private TabModel mTabModel;
    private Tab mTab;
    private HistoricalTabSaverImpl mHistoricalTabSaver;

    @Before
    public void setUp() {
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        mActivity = sActivityTestRule.getActivity();
        mTabModelSelector = mActivity.getTabModelSelector();
        mTabModel = mTabModelSelector.getModel(false);
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);
        mTab = mActivity.getActivityTab();
        ChromeTabUtils.waitForInteractable(mTab);
        mHistoricalTabSaver = new HistoricalTabSaverImpl(mTabModel);
    }

    @After
    public void tearDown() {
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);
        mHistoricalTabSaver.destroy();
    }

    /**
     * Tests saving a single unfrozen tab. Needs native to test saving a single tab to
     * TabRestoreService.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalTab_NotFrozen_HistoricalTabCreated() {
        sActivityTestRule.loadUrl(getUrl(TEST_PAGE_1));
        TabRestoreServiceUtils.createTabEntry(mTabModel, mTab);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(new HistoricalEntry(mTab));
        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving a single frozen tab. Needs native to test recovery of a frozen tab when saving
     * to TabRestoreService in native.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalTab_Frozen_HistoricalTabCreated() {
        final Tab tab =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab frozenTab = freezeTab(tab);
        // Clear the entry created by freezing the tab.
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);

        TabRestoreServiceUtils.createTabEntry(mTabModel, frozenTab);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(new HistoricalEntry(frozenTab));
        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving a single frozen tab that cannot be restored. Needs native to test handling of a
     * frozen tab that cannot be restored.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalTab_Frozen_CannotRestore() {
        final Tab tab =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab frozenTab = freezeTab(tab);

        runOnUiThreadBlocking(() -> TabTestUtils.setWebContentsState(frozenTab, null));
        // Clear the entry created by freezing the tab.
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);

        TabRestoreServiceUtils.createTabEntry(mTabModel, frozenTab);

        List<List<HistoricalEntry>> empty = new ArrayList<List<HistoricalEntry>>();
        assertEntriesAre(empty);
    }

    /** Tests saving a single group. Needs native to test saving a group to TabRestoreService. */
    @Test
    @MediumTest
    public void testCreateHistoricalGroup() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);

        HistoricalEntry group =
                new HistoricalEntry(
                        0,
                        null,
                        "Foo",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab0, tab1}));
        TabRestoreServiceUtils.createTabOrGroupEntry(mTabModel, group);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(group);
        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving a single frozen group. Needs native to test recovery of a frozen group when
     * saving to TabRestoreService in native.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalGroup_Frozen_HistoricalGroupCreated() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);

        selectFirstTab();
        final Tab frozenTab0 = freezeTab(tab0);
        final Tab frozenTab1 = freezeTab(tab1);

        // Clear the entry created by freezing the tab.
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);

        HistoricalEntry group =
                new HistoricalEntry(
                        0,
                        null,
                        "Foo",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {frozenTab0, frozenTab1}));
        TabRestoreServiceUtils.createTabOrGroupEntry(mTabModel, group);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(group);
        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving a single frozen group that cannot be restored. Needs native to test handling of
     * a frozen group that cannot be restored.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalGroup_Frozen_CannotRestore() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);

        selectFirstTab();
        final Tab frozenTab0 = freezeTab(tab0);
        final Tab frozenTab1 = freezeTab(tab1);

        runOnUiThreadBlocking(() -> TabTestUtils.setWebContentsState(frozenTab0, null));
        runOnUiThreadBlocking(() -> TabTestUtils.setWebContentsState(frozenTab1, null));
        // Clear the entry created by freezing the tab.
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);

        HistoricalEntry group =
                new HistoricalEntry(
                        0,
                        null,
                        "Foo",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {frozenTab0, frozenTab1}));
        TabRestoreServiceUtils.createTabOrGroupEntry(mTabModel, group);

        List<List<HistoricalEntry>> empty = new ArrayList<List<HistoricalEntry>>();
        assertEntriesAre(empty);
    }

    /**
     * Tests saving a single group with no title (default title == null). Needs native to test empty
     * title handling across the JNI boundary.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalGroup_EmptyTitle() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);

        HistoricalEntry group =
                new HistoricalEntry(
                        0, null, null, TabGroupColorId.GREY, Arrays.asList(new Tab[] {tab0, tab1}));
        TabRestoreServiceUtils.createTabOrGroupEntry(mTabModel, group);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(group);
        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving multiple tabs and groups in a window. Requires native to test adding a Window to
     * TabRestoreService.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalBulk_Mixed() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);
        final Tab tab2 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_3), /* incognito= */ false);
        final Tab tab3 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_4), /* incognito= */ false);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(new HistoricalEntry(tab0));
        expectedEntries.add(
                new HistoricalEntry(
                        1,
                        null,
                        "baz",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab1, tab2})));
        expectedEntries.add(new HistoricalEntry(tab3));
        TabRestoreServiceUtils.createWindowEntry(mTabModel, expectedEntries);

        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving a frozen bulk. Needs native to test recovery of a frozen bulk when saving to
     * TabRestoreService in native.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalBulk_Frozen_HistoricalBulkCreated() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);

        selectFirstTab();
        final Tab frozenTab0 = freezeTab(tab0);
        final Tab frozenTab1 = freezeTab(tab1);

        // Clear the entry created by freezing the tab.
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(new HistoricalEntry(frozenTab0));
        expectedEntries.add(new HistoricalEntry(frozenTab1));
        TabRestoreServiceUtils.createWindowEntry(mTabModel, expectedEntries);

        assertEntriesAre(Collections.singletonList(expectedEntries));
    }

    /**
     * Tests saving a frozen bulk that cannot be restored. Needs native to test handling of a frozen
     * bulk that cannot be restored.
     */
    @Test
    @MediumTest
    public void testCreateHistoricalBulk_Frozen_CannotRestore() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);

        selectFirstTab();
        final Tab frozenTab0 = freezeTab(tab0);
        final Tab frozenTab1 = freezeTab(tab1);

        runOnUiThreadBlocking(() -> TabTestUtils.setWebContentsState(frozenTab0, null));
        runOnUiThreadBlocking(() -> TabTestUtils.setWebContentsState(frozenTab1, null));
        // Clear the entry created by freezing the tab.
        TabRestoreServiceUtils.clearEntries(mTabModelSelector);

        ArrayList<HistoricalEntry> expectedEntries = new ArrayList<>();
        expectedEntries.add(new HistoricalEntry(frozenTab0));
        expectedEntries.add(new HistoricalEntry(frozenTab1));
        TabRestoreServiceUtils.createWindowEntry(mTabModel, expectedEntries);

        List<List<HistoricalEntry>> empty = new ArrayList<List<HistoricalEntry>>();
        assertEntriesAre(empty);
    }

    /** Tests saving a mix of entries in sequence. Requires native as a full end-to-end test. */
    @Test
    @MediumTest
    public void testCreateAllTypes() {
        final Tab tab0 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_1), /* incognito= */ false);
        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_2), /* incognito= */ false);
        final Tab tab2 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_3), /* incognito= */ false);
        final Tab tab3 =
                sActivityTestRule.loadUrlInNewTab(getUrl(TEST_PAGE_4), /* incognito= */ false);

        ArrayList<List<HistoricalEntry>> expectedEntries = new ArrayList<>();

        ArrayList<HistoricalEntry> window = new ArrayList<>();
        window.add(new HistoricalEntry(tab0));
        window.add(
                new HistoricalEntry(
                        5,
                        null,
                        "baz",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab1, tab2})));
        TabRestoreServiceUtils.createWindowEntry(mTabModel, window);
        expectedEntries.add(window);

        HistoricalEntry group =
                new HistoricalEntry(
                        3,
                        null,
                        "group",
                        TabGroupColorId.BLUE,
                        Arrays.asList(new Tab[] {tab3, tab2}));
        TabRestoreServiceUtils.createTabOrGroupEntry(mTabModel, group);
        expectedEntries.add(Arrays.asList(new HistoricalEntry[] {group}));

        HistoricalEntry tab = new HistoricalEntry(tab0);
        TabRestoreServiceUtils.createTabEntry(mTabModel, tab0);
        expectedEntries.add(Arrays.asList(new HistoricalEntry[] {tab}));

        assertEntriesAre(expectedEntries);
    }

    /** Tests invalid urls are not saved. Requires native for {@link GURL}. */
    @Test
    @MediumTest
    public void testCreateHistoricalTab_InvalidUrls() {
        List<List<HistoricalEntry>> empty = new ArrayList<List<HistoricalEntry>>();
        final Tab tab0 = sActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        TabRestoreServiceUtils.createTabEntry(mTabModel, tab0);
        assertEntriesAre(empty);

        final Tab tab1 =
                sActivityTestRule.loadUrlInNewTab("chrome://version/", /* incognito= */ false);
        TabRestoreServiceUtils.createTabEntry(mTabModel, tab1);
        assertEntriesAre(empty);

        final Tab tab2 =
                sActivityTestRule.loadUrlInNewTab(
                        "chrome-native://recent-tabs/", /* incognito= */ false);
        TabRestoreServiceUtils.createTabEntry(mTabModel, tab2);
        assertEntriesAre(empty);

        HistoricalEntry group =
                new HistoricalEntry(
                        0,
                        null,
                        "bar",
                        TabGroupColorId.GREY,
                        Arrays.asList(new Tab[] {tab1, tab2}));
        TabRestoreServiceUtils.createTabOrGroupEntry(mTabModel, group);
        assertEntriesAre(empty);

        TabRestoreServiceUtils.createWindowEntry(
                mTabModel, Arrays.asList(new HistoricalEntry[] {new HistoricalEntry(tab0), group}));
        assertEntriesAre(empty);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    public void testArchivedTabsAreExcluded() {
        ArchivedTabModelOrchestrator archivedTabModelOrchestrator =
                runOnUiThreadBlocking(
                        () ->
                                ArchivedTabModelOrchestrator.getForProfile(
                                        sActivityTestRule
                                                .getActivity()
                                                .getProfileProviderSupplier()
                                                .get()
                                                .getOriginalProfile()));
        CriteriaHelper.pollUiThread(() -> archivedTabModelOrchestrator.getTabArchiver() != null);

        Supplier<TabModel> archivedTabModelSupplier = archivedTabModelOrchestrator::getTabModel;
        mHistoricalTabSaver.addSecodaryTabModelSupplier(archivedTabModelSupplier);

        runOnUiThreadBlocking(
                () -> {
                    archivedTabModelOrchestrator
                            .getTabArchiver()
                            .archiveAndRemoveTab(mTabModel, mTabModel.getTabAt(0));
                });
        List<List<HistoricalEntry>> empty = new ArrayList<List<HistoricalEntry>>();
        assertEntriesAre(empty);

        runOnUiThreadBlocking(
                () -> {
                    archivedTabModelOrchestrator
                            .getTabArchiver()
                            .unarchiveAndRestoreTab(
                                    mActivity.getTabCreator(/* incognito= */ false),
                                    archivedTabModelSupplier.get().getTabAt(0));
                });
        assertEntriesAre(empty);
    }

    /**
     * @param expectedEntries A list of historical closures in the order added to the
     *     TabRestoreService.
     */
    private void assertEntriesAre(List<List<HistoricalEntry>> expectedEntries) {
        List<RecentlyClosedEntry> actualEntries =
                TabRestoreServiceUtils.getEntries(mTabModelSelector);

        // Reverse actual entries as it is sorted by most recent.
        Collections.reverse(actualEntries);

        Assert.assertEquals("Entry count mismatch.", expectedEntries.size(), actualEntries.size());
        for (int i = 0; i < expectedEntries.size(); ++i) {
            assert expectedEntries.get(i).size() != 0;

            if (expectedEntries.get(i).size() != 1) {
                assertBulkClosureEquals(i, expectedEntries.get(i), actualEntries.get(i));
                continue;
            }

            HistoricalEntry entry = expectedEntries.get(i).get(0);
            if (entry.isSingleTab()) {
                assertTabEquals(i, entry, actualEntries.get(i));
            } else {
                assertGroupEquals(i, entry, actualEntries.get(i));
            }
        }
    }

    private void assertTabEquals(
            int i, HistoricalEntry expectedTab, RecentlyClosedEntry recentEntry) {
        Assert.assertTrue(
                "Entry " + i + " is not a tab.", recentEntry instanceof RecentlyClosedTab);
        RecentlyClosedTab recentTab = (RecentlyClosedTab) recentEntry;
        Assert.assertEquals(
                "Entry " + i + " title mismatch.",
                ChromeTabUtils.getTitleOnUiThread(expectedTab.getTabs().get(0)),
                recentTab.getTitle());
        Assert.assertEquals(
                "Entry " + i + " url mismatch.",
                ChromeTabUtils.getUrlOnUiThread(expectedTab.getTabs().get(0)),
                recentTab.getUrl());
    }

    private void assertGroupEquals(
            int i, HistoricalEntry expectedGroup, RecentlyClosedEntry recentEntry) {
        Assert.assertTrue(
                "Entry " + i + " is not a group.", recentEntry instanceof RecentlyClosedGroup);
        RecentlyClosedGroup recentGroup = (RecentlyClosedGroup) recentEntry;

        // Reverse tabs as they are sorted by most recent.
        Collections.reverse(recentGroup.getTabs());

        Assert.assertEquals(
                "Entry " + i + " title mismatch.",
                expectedGroup.getGroupTitle() == null ? "" : expectedGroup.getGroupTitle(),
                recentGroup.getTitle());
        Assert.assertEquals(
                "Entry " + i + " tab count mismatch.",
                expectedGroup.getTabs().size(),
                recentGroup.getTabs().size());
        for (int j = 0; j < expectedGroup.getTabs().size(); j++) {
            Assert.assertEquals(
                    "Entry " + i + " tab " + j + " title mismatch.",
                    ChromeTabUtils.getTitleOnUiThread(expectedGroup.getTabs().get(j)),
                    recentGroup.getTabs().get(j).getTitle());
            Assert.assertEquals(
                    "Entry " + i + " tab " + j + " url mismatch.",
                    ChromeTabUtils.getUrlOnUiThread(expectedGroup.getTabs().get(j)),
                    recentGroup.getTabs().get(j).getUrl());
        }
    }

    private void assertBulkClosureEquals(
            int i, List<HistoricalEntry> entries, RecentlyClosedEntry recentEntry) {
        Assert.assertTrue(
                "Entry " + i + " is not a bulk closure.",
                recentEntry instanceof RecentlyClosedBulkEvent);
        RecentlyClosedBulkEvent recentBulk = (RecentlyClosedBulkEvent) recentEntry;
        List<Tab> expectedTabs = new ArrayList<>();
        Map<Tab, String> groupTitles = new HashMap<>();
        for (HistoricalEntry entry : entries) {
            if (entry.isSingleTab()) {
                expectedTabs.add(entry.getTabs().get(0));
                continue;
            }

            for (Tab tab : entry.getTabs()) {
                expectedTabs.add(tab);
                groupTitles.put(tab, entry.getGroupTitle() == null ? "" : entry.getGroupTitle());
            }
        }

        // Reverse tabs as they are sorted by most recent.
        Collections.reverse(recentBulk.getTabs());

        Assert.assertEquals(
                "Entry " + i + " group count mismatch.",
                new HashSet<String>(groupTitles.values()).size(),
                recentBulk.getTabGroupIdToTitleMap().size());
        for (int j = 0; j < expectedTabs.size(); j++) {
            Assert.assertEquals(
                    "Entry " + i + " tab " + j + " title mismatch.",
                    ChromeTabUtils.getTitleOnUiThread(expectedTabs.get(j)),
                    recentBulk.getTabs().get(j).getTitle());
            Assert.assertEquals(
                    "Entry " + i + " tab " + j + " url mismatch.",
                    ChromeTabUtils.getUrlOnUiThread(expectedTabs.get(j)),
                    recentBulk.getTabs().get(j).getUrl());
            Assert.assertEquals(
                    "Entry " + i + " tab " + j + " group mismatch.",
                    groupTitles.get(expectedTabs.get(j)),
                    recentBulk
                            .getTabGroupIdToTitleMap()
                            .get(recentBulk.getTabs().get(j).getTabGroupId()));
        }
    }

    private Tab freezeTab(Tab tab) {
        Tab[] frozen = new Tab[1];
        runOnUiThreadBlocking(
                () -> {
                    TabState state = TabStateExtractor.from(tab);
                    mActivity
                            .getCurrentTabModel()
                            .closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
                    frozen[0] =
                            mActivity.getCurrentTabCreator().createFrozenTab(state, tab.getId(), 1);
                });
        return frozen[0];
    }

    private String getUrl(String relativeUrl) {
        return sActivityTestRule.getTestServer().getURL(relativeUrl);
    }

    private void selectFirstTab() {
        runOnUiThreadBlocking(
                () -> {
                    mTabModelSelector.getCurrentModel().setIndex(0, TabSelectionType.FROM_USER);
                });
    }
}
