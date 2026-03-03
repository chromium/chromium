// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Holder;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;

import java.io.File;

/**
 * Integration tests for {@link PersistentStoreCleaner} via {@link TabModelOrchestrator} clearState.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
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
    public void cleanState_FullMigration() throws Exception {
        // 1. Configure Full Migration state before Activity start
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(true);
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
