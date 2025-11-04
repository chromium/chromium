// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation tests for {@link TabPersistentStoreImpl} reacting to events from TabModel and
 * Tab.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.PROCESS_RANK_POLICY_ANDROID})
@DisableFeatures({
    ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH,
    ChromeFeatureList.CHANGE_UNFOCUSED_PRIORITY,
})
@Batch(Batch.PER_CLASS)
public class TabPersistentStoreIntegrationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private TabModelSelector mTabModelSelector;
    private TabPersistentStoreImpl mTabPersistentStore;
    private String mTestUrl;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mTabPersistentStore =
                (TabPersistentStoreImpl)
                        mActivityTestRule
                                .getActivity()
                                .getTabModelOrchestratorSupplier()
                                .get()
                                .getTabPersistentStore();
        mTestUrl = mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/ok.txt");
    }

    private void waitForFile(File file, boolean exists) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(file.exists(), Matchers.is(exists));
                });
    }

    private void observeOnMetadataSaved(final CallbackHelper callbackHelper) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabPersistentStoreObserver observer =
                            new TabPersistentStoreObserver() {
                                @Override
                                public void onMetadataSavedAsynchronously() {
                                    callbackHelper.notifyCalled();
                                }
                            };
                    mTabPersistentStore.addObserver(observer);
                });
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testOpenAndCloseTabCreatesAndDeletesFile() throws Exception {
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        final int tabId = tab.getId();

        File tabStateFile = mTabPersistentStore.getTabStateFileForTesting(tabId, false);
        waitForFile(tabStateFile, true);

        final TabModel tabModel = mTabModelSelector.getModel(false);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabModel.getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(true).build(),
                                        /* allowDialog= */ false));

        assertTrue(tabStateFile.exists());

        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.commitTabClosure(tabId));

        waitForFile(tabStateFile, false);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testUndoTabClosurePersistsState() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        TabModel tabModel = mTabModelSelector.getModel(false);
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        final int tabId = tab.getId();

        onMetadataSaved.waitForCallback(saveCount);
        saveCount = onMetadataSaved.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabModel.getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(true).build(),
                                        /* allowDialog= */ false));
        onMetadataSaved.waitForCallback(saveCount);
        saveCount = onMetadataSaved.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.cancelTabClosure(tabId));
        onMetadataSaved.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testCloseTabPersistsState() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        TabModel tabModel = mTabModelSelector.getModel(false);
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);

        onMetadataSaved.waitForCallback(saveCount);
        saveCount = onMetadataSaved.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });
        onMetadataSaved.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testCloseAllTabsPersistsState() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        TabModel tabModel = mTabModelSelector.getModel(false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);

        onMetadataSaved.waitForCallback(saveCount);
        saveCount = onMetadataSaved.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs().allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });
        onMetadataSaved.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testSelectTabPersistsState() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        TabModel tabModel = mTabModelSelector.getModel(false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);

        onMetadataSaved.waitForCallback(saveCount);
        saveCount = onMetadataSaved.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.setIndex(0, TabSelectionType.FROM_USER);
                });
        onMetadataSaved.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testMoveTabPersistsState() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        TabModel tabModel = mTabModelSelector.getModel(false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        Tab tabToMove = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);

        onMetadataSaved.waitForCallback(saveCount);
        saveCount = onMetadataSaved.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.moveTab(tabToMove.getId(), 0);
                });
        onMetadataSaved.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH})
    public void testSkipSaveTabListDoesNotPersistMetadata() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        mTabPersistentStore.pauseSaveTabList();

        TabModel tabModel = mTabModelSelector.getModel(false);
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });

        assertEquals(saveCount, onMetadataSaved.getCallCount());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabPersistentStore.resumeSaveTabList();
                });

        onMetadataSaved.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH})
    public void testSkipSaveTabListDoesNotPersistMetadata_KillswitchDisabled() throws Exception {
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        int saveCount = onMetadataSaved.getCallCount();
        observeOnMetadataSaved(onMetadataSaved);

        mTabPersistentStore.pauseSaveTabList(); // should be a no-op

        TabModel tabModel = mTabModelSelector.getModel(false);
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });

        // Call will happen even while paused.
        onMetadataSaved.waitForCallback(saveCount);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabPersistentStore.resumeSaveTabList();
                });
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH})
    public void testSkipSaveTabList_ResumeRunnable() throws Exception {
        mTabPersistentStore.pauseSaveTabList();

        TabModel tabModel = mTabModelSelector.getModel(false);
        final Tab tab = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabModel.getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                });

        final CallbackHelper runnableCompleted = new CallbackHelper();
        int saveCount = runnableCompleted.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabPersistentStore.resumeSaveTabList(() -> runnableCompleted.notifyCalled());
                });

        runnableCompleted.waitForCallback(saveCount);
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    public void testUndoCloseAllTabsWritesTabListFile() throws Exception {
        // Create several tabs.
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        mActivityTestRule.loadUrlInNewTab(mTestUrl, false);

        TabModel regularModel = mTabModelSelector.getModel(false);
        assertEquals(3, (int) ThreadUtils.runOnUiThreadBlocking(() -> regularModel.getCount()));

        // Wait for saves to complete to avoid flakes.
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        observeOnMetadataSaved(onMetadataSaved);
        int saveCount = onMetadataSaved.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularModel.setIndex(regularModel.getCount() - 1, TabSelectionType.FROM_USER);
                });
        onMetadataSaved.waitForCallback(saveCount);

        // Close all tabs with undo enabled.
        closeAllTabsThenUndo(mTabModelSelector);

        // Synchronously save the data out to simulate minimizing Chrome.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabPersistentStore.saveState();
                });

        // Verify saved state.
        File dataDir = mTabPersistentStore.getStateDirectory();
        final var cipherFactory = new CipherFactory();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int j = 0; j < regularModel.getCount(); j++) {
                        Tab tab = regularModel.getTabAt(j);
                        TabState currentState =
                                TabStateFileManager.restoreTabState(
                                        dataDir,
                                        tab.getId(),
                                        cipherFactory,
                                        /* useFlatBuffer= */ true);
                        assertNotNull(currentState);
                        String expectedUrl = tab.getUrl().getSpec();
                        assertEquals(
                                expectedUrl, currentState.contentsState.getVirtualUrlFromState());
                    }
                });
    }

    private void closeAllTabsThenUndo(TabModelSelector selector) {
        TabModel regularModel = selector.getModel(false);
        final int tabCount = ThreadUtils.runOnUiThreadBlocking(() -> regularModel.getCount());
        final List<Integer> closedTabIds = new ArrayList<>();
        TabModelObserver closeObserver =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs,
                            boolean isAllTabs,
                            @TabClosingSource int closingSource) {
                        for (Tab tab : tabs) closedTabIds.add(tab.getId());
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularModel.addObserver(closeObserver);
                    regularModel
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeAllTabs().build(),
                                    /* allowDialog= */ false);
                });
        assertEquals(tabCount, closedTabIds.size());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Cancel closing each tab.
                    for (Integer id : closedTabIds) regularModel.cancelTabClosure(id);
                    regularModel.removeObserver(closeObserver);
                });
        assertEquals(
                tabCount, (int) ThreadUtils.runOnUiThreadBlocking(() -> regularModel.getCount()));
    }
}
