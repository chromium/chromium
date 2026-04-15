// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.createNewChromeTabbedActivity;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Holder;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.HeadlessTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

import java.io.File;

/**
 * Integration tests for {@link PersistentStoreCleaner} via {@link TabModelOrchestrator} clearState.
 */
@DoNotBatch(reason = "This testsuite creates and destroys activities.")
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE,
    ChromeFeatureList.SCHEDULE_WINDOW_CLEANING
})
public class PersistentStoreCleanerTest {
    private static final int WINDOW_ID = 0;
    private static final String WINDOW_TAG = "0";

    private static final String CURRENT_AUTHORITATIVE_STORE_KEY_1 =
            TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE.createKey(WINDOW_TAG);

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private Profile mProfile;
    private TabStateStorageService mService;
    private StorageLoadedData mLoadedData;

    @Before
    public void setUp() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
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
    public void cleanState_MigrationInProgress() throws Exception {
        // 1. Setup Phase
        File baseStateDir = TabStateDirectory.getOrCreateBaseStateDirectory();
        File windowStateDir = new File(baseStateDir, WINDOW_TAG);
        windowStateDir.mkdirs();

        String metadataFileName =
                TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(WINDOW_ID);
        File legacyMetadataFile = new File(windowStateDir, metadataFileName);

        if (!legacyMetadataFile.exists()) {
            assertTrue(legacyMetadataFile.createNewFile());
        }

        startActivityAndInitialize();

        StorageLoadedData initialData = loadAllDataSync(WINDOW_TAG, false);
        assertTrue(initialData.getLoadedTabStates().length > 0);

        // 2. Explicitly require the expected active states.
        Integer authoritative =
                runOnUiThreadBlocking(() -> getOrchestrator().getAuthoritativeStoreType());
        Integer shadow = runOnUiThreadBlocking(() -> getOrchestrator().getShadowStoreType());

        assertEquals(Integer.valueOf(StoreType.LEGACY), authoritative);
        assertEquals(Integer.valueOf(StoreType.TAB_STATE_STORE), shadow);

        // 3. Call clearState on the orchestrator.
        runOnUiThreadBlocking(() -> getOrchestrator().clearState());

        // 4. Explicitly wait for file deletion.
        CriteriaHelper.pollInstrumentationThread(() -> !legacyMetadataFile.exists());
        StorageLoadedData finalData = loadAllDataSync(WINDOW_TAG, false);
        assertEquals(0, finalData.getLoadedTabStates().length);
    }

    @Test
    @MediumTest
    public void cleanUnusedWindowsRemovesThumbnails() throws Exception {
        startActivityAndInitialize();

        Tab tab = runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().getActivityTab());
        @TabId int existingTabId = tab.getId();
        @TabId int nonExistingTabId = 99999;

        TabContentManager tabContentManager =
                runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().getTabContentManager());

        // Cache a thumbnail for the existing tab.
        runOnUiThreadBlocking(
                () -> {
                    tabContentManager.cacheTabThumbnailWithCallback(
                            tab, false, CallbackUtils.emptyCallback());
                });

        File existingTabFile = TabContentManager.getTabThumbnailFileJpeg(existingTabId);
        CriteriaHelper.pollInstrumentationThread(existingTabFile::exists);

        // Create a fake thumbnail file for a non-existing tab.
        File nonExistingTabFile = TabContentManager.getTabThumbnailFileJpeg(nonExistingTabId);
        assertTrue(nonExistingTabFile.createNewFile());
        assertTrue(nonExistingTabFile.exists());

        runOnUiThreadBlocking(
                () ->
                        PersistentStoreCleanerFactory.getForProfile(mProfile)
                                .scheduleCleanUnusedData(tabContentManager));
        CriteriaHelper.pollInstrumentationThread(() -> !nonExistingTabFile.exists());

        // Retrieve the cached thumbnail for the existing tab to ensure it wasn't deleted.
        existingTabFile = TabContentManager.getTabThumbnailFileJpeg(existingTabId);
        assertTrue(existingTabFile.exists());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(VERSION_CODES.S)
    public void cleanUnusedWindowsRemovesUnusedWindowData() throws Exception {
        startActivityAndInitialize();
        CriteriaHelper.pollUiThread(
                () -> TabWindowManagerSingleton.getInstance().isAllTabStateInitialized());

        TabWindowManager tabWindowManager =
                runOnUiThreadBlocking(TabWindowManagerSingleton::getInstance);

        // Create a fake directory for a non-existing window.
        int activeWindowId =
                tabWindowManager.getWindowIdForSelector(
                        mActivityTestRule.getActivity().getTabModelSelector());
        assertNotEquals(activeWindowId, INVALID_WINDOW_ID);

        File baseStateDir = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        File activeMetadataFile = new File(baseStateDir, "tab_state" + activeWindowId);
        CriteriaHelper.pollInstrumentationThread(activeMetadataFile::exists);

        TabContentManager tabContentManager =
                runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().getTabContentManager());

        MultiWindowUtils.setMaxInstancesForTesting(5);
        CallbackHelper initHelper = new CallbackHelper();
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        ChromeTabbedActivity unusedActivity = createNewChromeTabbedActivity(activity);
        TabModelSelector unusedTabModelSelector = unusedActivity.getTabModelSelector();
        runOnUiThreadBlocking(
                () ->
                        TabModelUtils.runOnTabStateInitialized(
                                initHelper::notifyCalled, unusedTabModelSelector));
        initHelper.waitForOnly();

        runOnUiThreadBlocking(
                () -> {
                    unusedTabModelSelector
                            .getModel(/* incognito= */ false)
                            .getTabCreator()
                            .launchUrl("about:blank", TabLaunchType.FROM_CHROME_UI);
                });
        int unusedWindowId = tabWindowManager.getWindowIdForSelector(unusedTabModelSelector);

        File unusedMetadataFile = new File(baseStateDir, "tab_state" + unusedWindowId);
        CriteriaHelper.pollInstrumentationThread(unusedMetadataFile::exists);

        CallbackHelper beforeCleanHelper = new CallbackHelper();
        Holder<Integer> beforeCountHolder = new Holder<>(null);
        runOnUiThreadBlocking(
                () ->
                        mService.countTabsForWindow(
                                String.valueOf(unusedWindowId),
                                /* isOffTheRecord= */ false,
                                count -> {
                                    beforeCountHolder.onResult(count);
                                    beforeCleanHelper.notifyCalled();
                                }));
        beforeCleanHelper.waitForOnly();
        assertEquals(Integer.valueOf(2), beforeCountHolder.get());

        unusedActivity.finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getStateForActivity(unusedActivity)
                                == ActivityState.DESTROYED);

        // After an activity is destroyed, the selector should become headless.
        CriteriaHelper.pollUiThread(
                () ->
                        tabWindowManager.getTabModelSelectorById(unusedWindowId)
                                instanceof HeadlessTabModelSelectorImpl);
        TabModelSelector headlessSelector =
                tabWindowManager.getTabModelSelectorById(unusedWindowId);

        // Force headless to shutdown to simulate orphaned data.
        runOnUiThreadBlocking(() -> tabWindowManager.shutdownIfHeadless(unusedWindowId));
        CriteriaHelper.pollUiThread(
                () -> !tabWindowManager.getAllTabModelSelectors().contains(headlessSelector));
        assertTrue(tabWindowManager.isAllTabStateInitialized());

        CriteriaHelper.pollUiThread(
                () ->
                        !PersistentStoreCleanerFactory.getForProfile(mProfile)
                                .hasUnusedDataDepsForTesting());

        runOnUiThreadBlocking(
                () ->
                        PersistentStoreCleanerFactory.getForProfile(mProfile)
                                .scheduleCleanUnusedData(tabContentManager));

        CriteriaHelper.pollUiThread(() -> !unusedMetadataFile.exists());

        CallbackHelper afterCleanHelper = new CallbackHelper();
        Holder<Integer> afterCountHolder = new Holder<>(null);
        runOnUiThreadBlocking(
                () ->
                        mService.countTabsForWindow(
                                String.valueOf(unusedWindowId),
                                /* isOffTheRecord= */ false,
                                count -> {
                                    afterCountHolder.onResult(count);
                                    afterCleanHelper.notifyCalled();
                                }));
        afterCleanHelper.waitForOnly();
        assertEquals(Integer.valueOf(0), afterCountHolder.get());
    }

    @Test
    @MediumTest
    public void cleanState_FullMigration() throws Exception {
        // 1. Configure Full Migration state before Activity start
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        File baseStateDir = TabStateDirectory.getOrCreateBaseStateDirectory();
        File windowStateDir = new File(baseStateDir, WINDOW_TAG);
        windowStateDir.mkdirs();

        String metadataFileName =
                TabbedModeTabPersistencePolicy.getMetadataFileNameForIndex(WINDOW_ID);
        File legacyMetadataFile = new File(windowStateDir, metadataFileName);

        if (!legacyMetadataFile.exists()) {
            assertTrue(legacyMetadataFile.createNewFile());
        }

        startActivityAndInitialize();

        StorageLoadedData initialData = loadAllDataSync(WINDOW_TAG, false);
        assertTrue(initialData.getLoadedTabStates().length > 0);

        // 2. Explicitly require the fully migrated state.
        Integer authoritative =
                runOnUiThreadBlocking(() -> getOrchestrator().getAuthoritativeStoreType());
        Integer shadow = runOnUiThreadBlocking(() -> getOrchestrator().getShadowStoreType());

        assertEquals(Integer.valueOf(StoreType.TAB_STATE_STORE), authoritative);
        assertNull(shadow);

        // 3. Call clearState on the orchestrator.
        runOnUiThreadBlocking(() -> getOrchestrator().clearState());

        // 4. Explicitly wait for file deletion.
        CriteriaHelper.pollInstrumentationThread(() -> !legacyMetadataFile.exists());
        StorageLoadedData finalData = loadAllDataSync(WINDOW_TAG, false);
        assertEquals(0, finalData.getLoadedTabStates().length);
    }

    private void startActivityAndInitialize() {
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
                });
    }

    private TabModelOrchestrator getOrchestrator() {
        return mActivityTestRule.getActivity().getTabModelOrchestratorSupplier().get();
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
}
