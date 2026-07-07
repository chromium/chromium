// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.app.tabmodel.PersistentStoreMigrationManagerImpl.MANAGER_VERSION;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_SHADOW_WRITTEN_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_STORE_MANAGER_VERSION;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Holder;
import org.chromium.base.Token;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabGroupCollectionData;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

/** Tests {@link TabStateStore} tracking a tabbed mode {@link TabModel}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Test interacts with activity shutdown and thus is incompatible with batching")
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
public class TabbedModeTabModelStoreTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final String WINDOW_TAG = "0";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private Profile mProfile;
    private TabStateStorageService mService;
    private TabModel mTabModel;
    private StorageLoadedData mLoadedData;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mService = TabStateStorageServiceFactory.getForProfile(mProfile);

                    TabModelSelector selector =
                            mActivityTestRule.getActivity().getTabModelSelector();
                    mTabModel = selector.getModel(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getTabModelSelector().isTabStateInitialized(),
                "Tab state never initialized");
    }

    @After
    public void tearDown() {
        if (mLoadedData != null) {
            destroyData(mLoadedData);
            mLoadedData = null;
        }
    }

    @Test
    @MediumTest
    public void tabAddition() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        @TabId int tabId = getActiveTabId();

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                found = true;
                break;
            }
        }
        assertTrue(found);
    }

    @Test
    @MediumTest
    public void tabUpdate() throws Exception {
        @TabId int tabId = getActiveTabId();
        Long newTimestamp = 123456789L;

        runOnUiThreadBlocking(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    tab.setTimestampMillis(newTimestamp);
                    tab.setIsPinned(true);
                    tab.setIsPinned(false);
                });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found = false;
        Long actualTimestamp = null;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                found = true;
                actualTimestamp = lts.tabState.timestampMillis;
                break;
            }
        }
        assertTrue("Updated tab should be found in storage", found);
        assertEquals("Tab does not match expected timestamp", newTimestamp, actualTimestamp);
    }

    @Test
    @MediumTest
    public void testUntidyDoesNotSave_DirtySaves() throws Exception {
        @TabId int tabId = getActiveTabId();

        // Get initial state from DB to know the baseline timestamp.
        StorageLoadedData data1 = loadAllDataSync(WINDOW_TAG, false);
        Long initialTimestamp = null;
        for (StorageLoadedData.LoadedTabState lts : data1.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                initialTimestamp = lts.tabState.timestampMillis;
                break;
            }
        }
        assertNotNull("Initial tab should have a timestamp in DB", initialTimestamp);

        // Change timestamp locally on the tab, and trigger UNTIDY.
        Long newTimestamp = initialTimestamp + 10000L; // 10s later
        runOnUiThreadBlocking(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    tab.setTimestampMillis(newTimestamp);

                    // Trigger UNTIDY state.
                    tab.setTabHasSensitiveContent(!tab.getTabHasSensitiveContent());
                });

        // Verify DB STILL has the initial timestamp (UNTIDY did not trigger save).
        StorageLoadedData data2 = loadAllDataSync(WINDOW_TAG, false);
        Long timestampAfterUntidy = null;
        for (StorageLoadedData.LoadedTabState lts : data2.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                timestampAfterUntidy = lts.tabState.timestampMillis;
                break;
            }
        }
        assertEquals(
                "UNTIDY update should not trigger save to DB",
                initialTimestamp,
                timestampAfterUntidy);

        // Trigger DIRTY state (e.g. via pin change).
        runOnUiThreadBlocking(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    // Trigger DIRTY state.
                    tab.setIsPinned(!tab.getIsPinned());
                });

        // Verify DB NOW has the new timestamp (DIRTY triggered save).
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        StorageLoadedData data3 = loadAllDataSync(WINDOW_TAG, false);
                        Long timestampAfterDirty = null;
                        for (StorageLoadedData.LoadedTabState lts : data3.getLoadedTabStates()) {
                            if (lts.tabId == tabId) {
                                timestampAfterDirty = lts.tabState.timestampMillis;
                                break;
                            }
                        }
                        Criteria.checkThat(
                                "DIRTY update should trigger save to DB",
                                timestampAfterDirty,
                                Matchers.is(newTimestamp));
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    @Test
    @MediumTest
    public void tabGroupCreation() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            mTabModel.mergeTabsToGroup(tabId2, tabId1);
                            return mTabModel.getTabById(tabId1).getTabGroupId();
                        });
        assertNotNull(groupId);

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found1 = false;
        Token actualGroupId1 = null;
        boolean found2 = false;
        Token actualGroupId2 = null;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId1) {
                found1 = true;
                actualGroupId1 = lts.tabState.tabGroupId;
            }
            if (lts.tabId == tabId2) {
                found2 = true;
                actualGroupId2 = lts.tabState.tabGroupId;
            }
        }
        assertTrue("Tab 1 not found in storage", found1);
        assertTrue("Tab 2 not found in storage", found2);
        assertEquals("Tab 1 group id not updated", groupId, actualGroupId1);
        assertEquals("Tab 2 group id not updated", groupId, actualGroupId2);
    }

    @Test
    @MediumTest
    public void tabGroupMetadataUpdate() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        String newTitle = "New Group Title";
        @TabGroupColorId Integer newColor = TabGroupColorId.BLUE;

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            mTabModel.mergeTabsToGroup(tabId2, tabId1);
                            Token id = mTabModel.getTabById(tabId1).getTabGroupId();
                            mTabModel.setTabGroupTitle(id, newTitle);
                            mTabModel.setTabGroupColor(id, newColor);
                            return id;
                        });

        boolean found = false;
        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        String actualTitle = null;
        @TabGroupColorId Integer actualColor = null;
        for (TabGroupCollectionData groupData : data.getGroupsData()) {
            if (groupId.equals(groupData.getTabGroupId())) {
                found = true;
                actualTitle = groupData.getTitle();
                actualColor = groupData.getColor();
                break;
            }
        }

        assertTrue("Stored group not found in storage", found);
        assertEquals("Stored group title not updated", newTitle, actualTitle);
        assertEquals("Stored group color not updated", newColor, actualColor);
    }

    @Test
    @MediumTest
    public void tabUngroup() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            mTabModel.mergeTabsToGroup(tabId2, tabId1);
                            Token id = mTabModel.getTabById(tabId1).getTabGroupId();
                            assertNotNull(id);
                            Tab tab2 = mTabModel.getTabById(tabId2);
                            mTabModel
                                    .getTabUngrouper()
                                    .ungroupTabs(
                                            List.of(tab2),
                                            /* trailing= */ true,
                                            /* allowDialog= */ false);
                            return id;
                        });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean tab1InGroup = false;
        boolean tab2InGroup = true;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId1) {
                tab1InGroup = groupId.equals(lts.tabState.tabGroupId);
            }
            if (lts.tabId == tabId2) {
                tab2InGroup = lts.tabState.tabGroupId != null;
            }
        }
        assertTrue("Tab 1 should still be in group", tab1InGroup);
        assertFalse("Tab 2 should be ungrouped", tab2InGroup);
    }

    @Test
    @MediumTest
    public void tabCloseAndUndo() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        @TabId int tabId = getActiveTabId();

        runOnUiThreadBlocking(
                () -> {
                    Tab tab = mTabModel.getTabById(tabId);
                    mTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(true).build(),
                                    /* allowDialog= */ false);
                    mTabModel.cancelTabClosure(tabId);
                });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                found = true;
                break;
            }
        }
        assertTrue("Undone tab should be back in storage", found);
    }

    @Test
    @MediumTest
    public void tabPinUpdate() throws Exception {
        @TabId int tabId = getActiveTabId();

        runOnUiThreadBlocking(() -> mTabModel.pinTab(tabId, false));

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean isPinned = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                isPinned = lts.tabState.isPinned;
                break;
            }
        }
        assertTrue("Tab should be marked as pinned in storage", isPinned);
    }

    @Test
    @MediumTest
    public void tabGroupCloseAndUndo() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            mTabModel.mergeTabsToGroup(tabId2, tabId1);

                            Token groupIdInternal = mTabModel.getTabById(tabId1).getTabGroupId();
                            assertNotNull(groupIdInternal);

                            TabClosureParams params =
                                    TabClosureParams.forCloseTabGroup(mTabModel, groupIdInternal)
                                            .allowUndo(true)
                                            .build();
                            mTabModel.getTabRemover().closeTabs(params, /* allowDialog= */ false);

                            mTabModel.cancelTabClosure(tabId1);
                            mTabModel.cancelTabClosure(tabId2);
                            return groupIdInternal;
                        });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found1 = false;
        Token actualGroupId1 = null;
        boolean found2 = false;
        Token actualGroupId2 = null;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId1) {
                found1 = true;
                actualGroupId1 = lts.tabState.tabGroupId;
            }
            if (lts.tabId == tabId2) {
                found2 = true;
                actualGroupId2 = lts.tabState.tabGroupId;
            }
        }
        assertTrue("Tab 1 not found in storage", found1);
        assertTrue("Tab 2 not found in storage", found2);
        assertEquals("Tab 1 group id not updated", groupId, actualGroupId1);
        assertEquals("Tab 2 group id not updated", groupId, actualGroupId2);
    }

    @Test
    @MediumTest
    public void tabGroupCollapsed() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            mTabModel.mergeTabsToGroup(tabId2, tabId1);
                            Token id = mTabModel.getTabById(tabId1).getTabGroupId();
                            mTabModel.setTabGroupCollapsed(id, true);
                            return id;
                        });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean isCollapsed = false;
        for (TabGroupCollectionData groupData : data.getGroupsData()) {
            if (groupId.equals(groupData.getTabGroupId())) {
                isCollapsed = groupData.isCollapsed();
                break;
            }
        }
        assertTrue("Group should be collapsed in storage", isCollapsed);
    }

    @Test
    @MediumTest
    public void tabRemoval() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        @TabId int tabId = getActiveTabId();

        runOnUiThreadBlocking(
                () -> {
                    Tab tab = mTabModel.getTabById(tabId);
                    mTabModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId) {
                found = true;
                break;
            }
        }
        assertFalse("Tab should be removed from storage", found);
    }

    @Test
    @MediumTest
    public void tabGroupRemoval() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        int count = runOnUiThreadBlocking(() -> mTabModel.getCount());
        assertEquals(2, count);

        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            mTabModel.mergeTabsToGroup(tabId2, tabId1);
                            Token id = mTabModel.getTabById(tabId1).getTabGroupId();
                            assertNotNull(id);
                            TabClosureParams params =
                                    TabClosureParams.forCloseTabGroup(mTabModel, id)
                                            .allowUndo(false)
                                            .build();
                            mTabModel.getTabRemover().closeTabs(params, /* allowDialog= */ false);
                            return id;
                        });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean tab1Found = false;
        boolean tab2Found = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId1) tab1Found = true;
            if (lts.tabId == tabId2) tab2Found = true;
        }
        boolean groupFound = false;
        for (TabGroupCollectionData groupData : data.getGroupsData()) {
            if (groupId.equals(groupData.getTabGroupId())) {
                groupFound = true;
                break;
            }
        }
        assertFalse(tab1Found);
        assertFalse(tab2Found);
        assertFalse(groupFound);
    }

    @Test
    @MediumTest
    public void activeTabPersistence() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        int count = runOnUiThreadBlocking(() -> mTabModel.getCount());
        assertEquals(2, count);

        runOnUiThreadBlocking(() -> mTabModel.setIndex(0, TabSelectionType.FROM_USER));

        StorageLoadedData data0 = loadAllDataSync(WINDOW_TAG, false);
        assertEquals(0, data0.getActiveTabIndex());

        runOnUiThreadBlocking(() -> mTabModel.setIndex(1, TabSelectionType.FROM_USER));

        StorageLoadedData data1 = loadAllDataSync(WINDOW_TAG, false);
        assertEquals(1, data1.getActiveTabIndex());
    }

    @Test
    @MediumTest
    public void tabMove() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId0 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());

        runOnUiThreadBlocking(() -> mTabModel.moveTab(tabId0, 1));

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        StorageLoadedData.LoadedTabState[] states = data.getLoadedTabStates();
        assertEquals(2, states.length);
        assertEquals(tabId1, states[0].tabId);
        assertEquals(tabId0, states[1].tabId);
    }

    @Test
    @MediumTest
    public void tabGroupMove() throws Exception {
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);

        @TabId int tabId0 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(0).getId());
        @TabId int tabId1 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(1).getId());
        @TabId int tabId2 = runOnUiThreadBlocking(() -> mTabModel.getTabAt(2).getId());

        runOnUiThreadBlocking(
                () -> {
                    mTabModel.mergeTabsToGroup(tabId1, tabId0);
                    mTabModel.moveRelatedTabs(tabId0, 2);
                });

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        StorageLoadedData.LoadedTabState[] states = data.getLoadedTabStates();
        assertEquals(3, states.length);
        assertEquals(tabId2, states[0].tabId);
        assertEquals(tabId0, states[1].tabId);
        assertEquals(tabId1, states[2].tabId);
    }

    @Test
    @MediumTest
    public void singleTabGroup() throws Exception {
        @TabId int tabId = getActiveTabId();

        Token groupId =
                runOnUiThreadBlocking(
                        () -> {
                            Tab tab = mTabModel.getTabById(tabId);
                            mTabModel.createSingleTabGroup(tab);
                            return mTabModel.getTabById(tabId).getTabGroupId();
                        });
        assertNotNull(groupId);

        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean found = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == tabId && groupId.equals(lts.tabState.tabGroupId)) {
                found = true;
                break;
            }
        }
        assertTrue("Single tab group should be persisted", found);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE
                    + ":phase/"
                    + TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE)
    public void testAuthoritativeStoreAppLoadAndRestart() throws Exception {
        setupActivityStore(StoreType.TAB_STATE_STORE);

        TabModelOrchestrator orchestrator =
                mActivityTestRule.getActivity().getTabModelOrchestratorSupplier().get();
        assertEquals(
                Integer.valueOf(StoreType.TAB_STATE_STORE),
                runOnUiThreadBlocking(orchestrator::getAuthoritativeStoreType));
        assertTrue(orchestrator.getTabPersistentStore() instanceof TabStateStore);

        // Load a new tab to change state.
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        @TabId int newTabId = getActiveTabId();

        // Save state and recreate activity.
        runOnUiThreadBlocking(() -> orchestrator.saveState());

        mActivityTestRule.recreateActivity();

        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        TabModelSelector newSelector = newActivity.getTabModelSelector();

        CriteriaHelper.pollUiThread(
                newSelector::isTabStateInitialized,
                "Recreated activity tab state never initialized");

        TabModel newModel = runOnUiThreadBlocking(() -> newSelector.getModel(false));
        CriteriaHelper.pollUiThread(
                () -> runOnUiThreadBlocking(() -> newModel.getCount() >= 2),
                "Tabs were not properly restored on app load");

        // Verify restored tab state.
        assertNotNull(
                "Newly added tab should be restored after activity restart",
                runOnUiThreadBlocking(() -> newModel.getTabById(newTabId)));

        // Verify data in TabStateStorageService DB matches.
        StorageLoadedData data = loadAllDataSync(WINDOW_TAG, false);
        boolean foundInDb = false;
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabId == newTabId) {
                foundInDb = true;
                break;
            }
        }
        assertTrue("Newly added tab should exist in TabStateStorageService DB", foundInDb);
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE
                    + ":phase/"
                    + TabStateStorageFlagHelper.PHASE_ONLY_SHADOW)
    public void testNonAuthoritativeStoreAppLoadAndRestart() throws Exception {
        setupActivityStore(StoreType.LEGACY);

        TabModelOrchestrator orchestrator =
                mActivityTestRule.getActivity().getTabModelOrchestratorSupplier().get();
        assertEquals(
                Integer.valueOf(StoreType.LEGACY),
                runOnUiThreadBlocking(orchestrator::getAuthoritativeStoreType));
        assertEquals(
                Integer.valueOf(StoreType.TAB_STATE_STORE),
                runOnUiThreadBlocking(orchestrator::getShadowStoreType));

        // Load a new tab to change state.
        mActivityTestRule.loadUrlInNewTab(
                mActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
        @TabId int newTabId = getActiveTabId();

        // Save state and recreate activity.
        runOnUiThreadBlocking(() -> orchestrator.saveState());

        mActivityTestRule.recreateActivity();

        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        TabModelSelector newSelector = newActivity.getTabModelSelector();

        CriteriaHelper.pollUiThread(
                newSelector::isTabStateInitialized,
                "Recreated activity tab state never initialized");

        TabModel newModel = runOnUiThreadBlocking(() -> newSelector.getModel(false));
        CriteriaHelper.pollUiThread(
                () -> runOnUiThreadBlocking(() -> newModel.getCount() >= 2),
                "Tabs were not properly restored on shadow app load");

        // Verify restored tab state.
        assertNotNull(
                "Newly added tab should be restored after shadow activity restart",
                runOnUiThreadBlocking(() -> newModel.getTabById(newTabId)));
    }

    private void destroyData(StorageLoadedData data) {
        for (StorageLoadedData.LoadedTabState lts : data.getLoadedTabStates()) {
            if (lts.tabState != null && lts.tabState.contentsState != null) {
                lts.tabState.contentsState.destroy();
            }
        }
        data.destroy();
    }

    private StorageLoadedData loadAllDataSync(String windowTag, boolean incognito)
            throws Exception {
        if (mLoadedData != null) {
            destroyData(mLoadedData);
            mLoadedData = null;
        }
        Holder<StorageLoadedData> holder = new Holder<>(null);
        CallbackHelper helper = new CallbackHelper();
        runOnUiThreadBlocking(
                () ->
                        mService.loadAllData(
                                windowTag,
                                incognito,
                                data -> {
                                    holder.onResult(data);
                                    helper.notifyCalled();
                                }));
        helper.waitForCallback(0);
        mLoadedData = holder.get();
        assertNotNull(mLoadedData);
        return mLoadedData;
    }

    private @TabId int getActiveTabId() {
        return runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getActivityTabProvider().get().getId());
    }

    private void setupActivityStore(@StoreType int authoritativeStoreType) {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(TAB_PERSISTENCE_STORE_MANAGER_VERSION, MANAGER_VERSION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(
                        TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE.createKey(WINDOW_TAG),
                        authoritativeStoreType);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(
                        TAB_PERSISTENCE_SHADOW_WRITTEN_STORE.createKey(WINDOW_TAG),
                        StoreType.UNKNOWN);

        mActivityTestRule.recreateActivity();

        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mService = TabStateStorageServiceFactory.getForProfile(mProfile);
                    mTabModel =
                            mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
                });

        CriteriaHelper.pollUiThread(
                () -> mActivityTestRule.getActivity().getTabModelSelector().isTabStateInitialized(),
                "Tab state never initialized");
    }
}
