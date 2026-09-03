// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Instrumentation tests for {@link TabPersistentStoreImpl} reacting to events from TabModel and
 * Tab.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(b/555414915): Update Android tests with WebUI NTP enabled on AL.
@DisableFeatures({
    ChromeFeatureList.CHANGE_UNFOCUSED_PRIORITY,
    ChromeFeatureList.USE_WEB_UI_NTP_ANDROID
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

        CriteriaHelper.pollUiThread(
                () -> {
                    Profile profile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    ArchivedTabModelOrchestrator orchestrator =
                            ArchivedTabModelOrchestrator.getForProfile(profile);
                    return orchestrator != null && orchestrator.isTabModelInitialized();
                });
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
        observeOnMetadataSaved(mTabPersistentStore, callbackHelper);
    }

    private void observeOnMetadataSaved(
            final TabPersistentStore store, final CallbackHelper callbackHelper) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabPersistentStoreObserver observer =
                            new TabPersistentStoreObserver() {
                                @Override
                                public void onMetadataSavedAsynchronously() {
                                    callbackHelper.notifyCalled();
                                }
                            };
                    store.addObserver(observer);
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

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    @RequiresRestart("Multiple activities make this complicated without a restart.")
    public void testFallbackNtpRestorationOrder() throws Exception {
        // 1. Start with 1 tab (blank). Load ok.txt in a new tab.
        final Tab tab0 = mActivityTestRule.loadUrlInNewTab(mTestUrl, false);
        // Now we have [Blank, tab0]. tab0 is active.

        // Close the blank tab to simplify.
        final TabModel normalModel = mTabModelSelector.getModel(false);
        final Tab blankTab = ThreadUtils.runOnUiThreadBlocking(() -> normalModel.getTabAt(0));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        normalModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(blankTab)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));
        // Now we have only [tab0].

        // 2. Open NTP.
        final CallbackHelper onMetadataSaved = new CallbackHelper();
        observeOnMetadataSaved(onMetadataSaved);
        int saveCount = onMetadataSaved.getCallCount();

        final Tab tab1 =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivityTestRule
                                    .getActivity()
                                    .getTabCreator(false)
                                    .launchUrl("chrome://newtab/", TabLaunchType.FROM_CHROME_UI);
                        });
        // Now we have [tab0, tab1]. tab1 (NTP) is active.
        int count = ThreadUtils.runOnUiThreadBlocking(() -> normalModel.getCount());
        Tab t0 = ThreadUtils.runOnUiThreadBlocking(() -> normalModel.getTabAt(0));
        Tab t1 = ThreadUtils.runOnUiThreadBlocking(() -> normalModel.getTabAt(1));
        Tab current =
                ThreadUtils.runOnUiThreadBlocking(() -> TabModelUtils.getCurrentTab(normalModel));
        assertEquals(2, count);
        assertEquals(tab0, t0);
        assertEquals(tab1, t1);
        assertEquals(tab1, current);

        // 3. Wait for metadata to save.
        onMetadataSaved.waitForCallback(saveCount);

        // Ensure TabState for tab0 is saved.
        File tab0StateFile = mTabPersistentStore.getTabStateFileForTesting(tab0.getId(), false);
        waitForFile(tab0StateFile, true);

        // NTP (tab1) might have a state file if it was saved.
        // We want to make sure it does NOT exist to force fallback.
        File tab1StateFile = mTabPersistentStore.getTabStateFileForTesting(tab1.getId(), false);
        if (tab1StateFile.exists()) {
            tab1StateFile.delete();
        }

        // 4. Recreate the activity!
        mActivityTestRule.recreateActivity();

        // 5. Get the new activity and models.
        org.chromium.chrome.browser.ChromeTabbedActivity newActivity =
                mActivityTestRule.getActivity();
        TabModelSelector newSelector = newActivity.getTabModelSelector();

        // 6. Wait for restoration to complete.
        CriteriaHelper.pollUiThread(newSelector::isTabStateInitialized);

        // Retrieve the model AFTER initialization is complete to avoid getting EmptyTabModel.
        final TabModel newNormalModel =
                ThreadUtils.runOnUiThreadBlocking(() -> newSelector.getModel(false));

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(newNormalModel.getCount(), Matchers.is(2));
                });

        // 7. Verify!
        Tab restoredTab0 = ThreadUtils.runOnUiThreadBlocking(() -> newNormalModel.getTabAt(0));
        Tab restoredTab1 = ThreadUtils.runOnUiThreadBlocking(() -> newNormalModel.getTabAt(1));
        org.chromium.url.GURL url0 = ThreadUtils.runOnUiThreadBlocking(() -> restoredTab0.getUrl());
        org.chromium.url.GURL url1 = ThreadUtils.runOnUiThreadBlocking(() -> restoredTab1.getUrl());
        Tab restoredCurrent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> TabModelUtils.getCurrentTab(newNormalModel));

        assertEquals(mTestUrl, url0.getSpec());
        assertTrue(
                url1.getSpec().startsWith("chrome://newtab")
                        || url1.getSpec().startsWith("chrome-native://newtab"));

        assertEquals(restoredTab1, restoredCurrent);

        // Verify that NTP was restored from disk (gets a new ID) and not just reused in memory.
        assertTrue(tab1.getId() != restoredTab1.getId());
    }

    @Test
    @MediumTest
    @Feature({"TabPersistentStore"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @RequiresRestart("Multiple activities make this complicated without a restart.")
    public void testMultiWindowFallbackNtpRestorationOrder() throws Exception {
        final ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();

        ChromeTabbedActivity activity2 = null;
        ChromeTabbedActivity recreatedActivity2 = null;
        int windowId2 = TabWindowManager.INVALID_WINDOW_ID;
        try {
            // 1. Start second activity (Activity2).
            if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
                activity2 = MultiWindowTestHelper.createNewChromeTabbedActivity(activity1);
            } else {
                activity2 = MultiWindowTestHelper.createSecondChromeTabbedActivity(activity1, null);
            }

            final ChromeTabbedActivity finalActivity2 = activity2;
            CriteriaHelper.pollUiThread(
                    () -> {
                        return finalActivity2.areTabModelsInitialized()
                                && finalActivity2.getTabModelSelector().isTabStateInitialized();
                    });

            final TabModel model2 =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> finalActivity2.getTabModelSelector().getModel(false));
            int count2 = ThreadUtils.runOnUiThreadBlocking(() -> model2.getCount());
            assertEquals(1, count2);
            final Tab tab0W2 = ThreadUtils.runOnUiThreadBlocking(() -> model2.getTabAt(0));
            String testUrl2 = UrlUtils.encodeHtmlDataUri("<html>test_url_2.</html>");
            ChromeTabUtils.loadUrlOnUiThread(tab0W2, testUrl2);
            ChromeTabUtils.waitForTabPageLoaded(tab0W2, testUrl2);

            // 2. Open NTP in Activity2.
            final TabModelOrchestrator orchestrator2 =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> finalActivity2.getTabModelOrchestratorSupplier().get());
            final TabPersistentStoreImpl store2 =
                    (TabPersistentStoreImpl) orchestrator2.getTabPersistentStore();
            final CallbackHelper onMetadataSavedW2 = new CallbackHelper();
            observeOnMetadataSaved(store2, onMetadataSavedW2);
            int saveCount = onMetadataSavedW2.getCallCount();

            final Tab tab1W2 =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                return finalActivity2
                                        .getTabCreator(false)
                                        .launchUrl(
                                                "chrome://newtab/", TabLaunchType.FROM_CHROME_UI);
                            });
            // Now we have [tab0W2, tab1W2]. tab1W2 (NTP) is active.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        assertEquals(2, model2.getCount());
                        assertEquals(tab0W2, model2.getTabAt(0));
                        assertEquals(tab1W2, model2.getTabAt(1));
                        assertEquals(tab1W2, TabModelUtils.getCurrentTab(model2));
                    });

            // 3. Wait for metadata to save for Activity2.
            onMetadataSavedW2.waitForCallback(saveCount);

            // Ensure TabState for tab0W2 is saved.
            File tab0StateFile = store2.getTabStateFileForTesting(tab0W2.getId(), false);
            waitForFile(tab0StateFile, true);

            // Ensure NTP (tab1W2) does NOT have a state file.
            File tab1StateFile = store2.getTabStateFileForTesting(tab1W2.getId(), false);
            if (tab1StateFile.exists()) {
                tab1StateFile.delete();
            }

            // 4. Recreate or Relaunch Activity2!
            if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
                final int finalWindowId2 =
                        ThreadUtils.runOnUiThreadBlocking(
                                () ->
                                        TabWindowManagerSingleton.getInstance()
                                                .getIdForWindow(finalActivity2));
                windowId2 = finalWindowId2;

                // Finish the activity first.
                ApplicationTestUtils.finishActivity(finalActivity2);

                // Relaunch it (Cold Start).
                recreatedActivity2 =
                        ApplicationTestUtils.waitForActivityWithClass(
                                ChromeTabbedActivity.class,
                                Stage.RESUMED,
                                () -> {
                                    ThreadUtils.runOnUiThreadBlocking(
                                            () -> {
                                                MultiWindowUtils.relaunchChromeTabbedActivity2(
                                                        ContextUtils.getApplicationContext(),
                                                        finalWindowId2,
                                                        null);
                                            });
                                });
            } else {
                recreatedActivity2 = ApplicationTestUtils.recreateActivity(activity2);
            }

            // 5. Wait for TabState to be initialized in recreated Activity2.
            final ChromeTabbedActivity finalRecreatedActivity2 = recreatedActivity2;
            if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
                final int finalWindowId2 = windowId2;
                final int newWindowId2 =
                        ThreadUtils.runOnUiThreadBlocking(
                                () ->
                                        TabWindowManagerSingleton.getInstance()
                                                .getIdForWindow(finalRecreatedActivity2));
                assertEquals(finalWindowId2, newWindowId2);
            }

            final TabModelSelector newSelector2 = recreatedActivity2.getTabModelSelector();
            CriteriaHelper.pollUiThread(() -> newSelector2.isTabStateInitialized(), 10000, 100);

            final TabModel newNormalModel2 = newSelector2.getModel(false);
            // We expect 2 tabs.
            CriteriaHelper.pollUiThread(() -> newNormalModel2.getCount() == 2, 10000, 100);

            // 6. Verify!
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Tab r0 = newNormalModel2.getTabAt(0);
                        Tab r1 = newNormalModel2.getTabAt(1);
                        assertEquals(testUrl2, r0.getUrl().getSpec());
                        assertTrue(UrlUtilities.isNtpUrl(r1.getUrl()));
                        assertEquals(r1, TabModelUtils.getCurrentTab(newNormalModel2));
                        assertTrue(tab1W2.getId() != r1.getId());
                    });
        } finally {
            if (recreatedActivity2 != null) {
                ApplicationTestUtils.finishActivity(recreatedActivity2);
            } else if (activity2 != null) {
                ApplicationTestUtils.finishActivity(activity2);
            }
        }
    }
}
