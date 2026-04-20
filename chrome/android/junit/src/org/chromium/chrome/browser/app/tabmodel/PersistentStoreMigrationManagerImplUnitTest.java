// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.app.tabmodel.PersistentStoreMigrationManagerImpl.MANAGER_VERSION;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_SHADOW_WRITTEN_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_STORE_MANAGER_VERSION;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;

/** Unit tests for {@link PersistentStoreMigrationManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
public class PersistentStoreMigrationManagerImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String WINDOW_TAG_1 = "window_1";
    private static final String WINDOW_TAG_2 = "window_2";

    private static final String CURRENT_AUTHORITATIVE_STORE_KEY_1 =
            TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE.createKey(WINDOW_TAG_1);
    private static final String SHADOW_WRITTEN_STORE_KEY_1 =
            TAB_PERSISTENCE_SHADOW_WRITTEN_STORE.createKey(WINDOW_TAG_1);
    private static final String CURRENT_AUTHORITATIVE_STORE_KEY_2 =
            TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE.createKey(WINDOW_TAG_2);
    private static final String SHADOW_WRITTEN_STORE_KEY_2 =
            TAB_PERSISTENCE_SHADOW_WRITTEN_STORE.createKey(WINDOW_TAG_2);

    private PersistentStoreMigrationManagerImpl mManager;

    @Before
    public void setUp() {
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE);
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(TAB_PERSISTENCE_SHADOW_WRITTEN_STORE);
        ChromeSharedPreferences.getInstance().removeKey(TAB_PERSISTENCE_STORE_MANAGER_VERSION);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testSteadyState_Legacy() {
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testShadowState_LegacyToTabStateStore() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.LEGACY);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());

        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();

        assertTrue(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testReadyState_LegacyToTabStateStore() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);

        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.LEGACY);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());

        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();

        // On restart we will switch the authoritative store.
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertTrue(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testSwitchedState_LegacyToTabStateStore() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);

        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.LEGACY);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());

        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();

        // Create a new manager instance to simulate restart.
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    public void testSteadyState_TabStateStore() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    public void testShadowState_TabStateStoreToLegacy() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());

        mManager.onShadowStoreCreated(StoreType.LEGACY);
        assertFalse(mManager.isShadowStoreCaughtUp());

        mManager.onShadowStoreCaughtUp();
        assertTrue(mManager.isShadowStoreCaughtUp());

        // Won't rollback until TabStateStore authoritative reads are disabled.
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    public void testReadyAndSwitchedState_TabStateStoreToLegacy() {
        // Setup TabStateStore as authoritative with Legacy shadow caught up
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        // Restart with authoritative read source DISABLED (revert to Legacy)
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
    }

    @Test
    public void testMigration_Rollback() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        // Rollback to Legacy.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());

        // Rollback to TabStateStore.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testMigration_ShadowStoreRazedWhenFeatureDisabled() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        // Rollback to Legacy with no shadow.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());

        // Shadow store caught up status should be reset.
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));
    }

    @Test
    public void testOnShadowStoreRazed() {
        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();
        assertEquals(
                StoreType.TAB_STATE_STORE,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));

        mManager.onShadowStoreRazed();

        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testOnAllStoresRazed() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_2, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_2, StoreType.LEGACY);

        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();

        assertEquals(
                StoreType.TAB_STATE_STORE,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));
        assertEquals(
                StoreType.LEGACY,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_2, StoreType.INVALID));
        assertEquals(
                StoreType.TAB_STATE_STORE,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_2, StoreType.INVALID));

        mManager.onAllStoresRazed();

        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_2, StoreType.INVALID));
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_2, StoreType.INVALID));
    }

    @Test
    public void testOnWindowCleared() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        assertEquals(
                StoreType.TAB_STATE_STORE,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.INVALID));
        assertEquals(
                StoreType.LEGACY,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));

        mManager.onWindowCleared();
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.INVALID));
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(SHADOW_WRITTEN_STORE_KEY_1, StoreType.INVALID));
    }

    @Test
    public void testNoRepeatedSwap() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());

        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        // Swap happened.
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertTrue(mManager.isShadowStoreCaughtUp());

        // Simulate another catch up. Store should not be swapped.
        mManager.onShadowStoreCaughtUp();
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertTrue(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testShadowState_FullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
    }

    @Test
    public void testLegacyShadowAfterFullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    public void testMigration_RollbackFromFullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        // Start Rollback. Start shadowing as legacy.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());

        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        // Start Rollback in next session. Legacy should now be authoritative.
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
    }

    @Test
    public void testMigration_FullRollback() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        // Start full rollback. LEGACY should shadow to catch up and TAB_STATE_STORE should be
        // authoritative.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_ROLLBACK);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());

        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        // Start Rollback in next session. Legacy should now be authoritative.
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testMigration_ForceFullRollback() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        // Should fully rollback to Legacy without catchup phase.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
    }

    @Test
    public void testShouldRazeShadowStoreForWindow() {
        assertFalse(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ false));

        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();
        assertFalse(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ false));

        mManager.onShadowStoreRazed();
        assertTrue(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ false));
    }

    @Test
    public void testShouldRazeShadowStoreForWindow_FullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        mManager.onAuthoritativeStoreInitialized(StoreType.TAB_STATE_STORE);

        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());

        // Start a rollback. Begin Shadowing.
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_ONLY_SHADOW);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());

        // Verify that the shadow store should be razed.
        assertTrue(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ false));

        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        // Complete Rollback.
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
    }

    @Test
    public void testOnAuthoritativeStoreInitialized() {
        assertFalse(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_1));

        mManager.onAuthoritativeStoreInitialized(StoreType.TAB_STATE_STORE);

        assertEquals(
                StoreType.TAB_STATE_STORE,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.INVALID));
    }

    @Test
    public void testShouldRazeStoreForWindow_Authoritative() {
        assertFalse(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ true));

        mManager.onAuthoritativeStoreInitialized(StoreType.TAB_STATE_STORE);
        assertFalse(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ true));

        mManager.onWindowCleared();
        assertTrue(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ true));
    }

    @Test
    public void testDefaultStore_TabStateStorageEnabled() {
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultStore_AuthoritativeReadsEnabled() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultStore_FullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testDefaultAfterClear_TabStateStorageDisabled() {
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());

        mManager.onWindowCleared();
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.INVALID));

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultAfterClear_TabStateStorageEnabled() {
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());

        mManager.onWindowCleared();
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.INVALID));

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultAfterClear_AuthoritativeReadsEnabled() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());

        mManager.onWindowCleared();
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultAfterClear_FullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_FULL_MIGRATION);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());

        mManager.onWindowCleared();
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testGetAuthoritativeStoreType_UnknownWithLegacyDefault() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.UNKNOWN);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
    }

    @Test
    public void testGetAuthoritativeStoreType_UnknownWithTabStateStoreDefault() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.UNKNOWN);
        ChromeFeatureList.sTabStorageSqlitePrototypePhase.setForTesting(
                TabStateStorageFlagHelper.PHASE_AUTHORITATIVE_READ_SOURCE);
        // With flag true, fallback should be TAB_STATE_STORE.
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
    }

    @Test
    public void testIsShadowStoreCaughtUp_Unknown() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.UNKNOWN);
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testShouldRazeStoreForWindow_UnknownAuthoritative() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.UNKNOWN);
        assertTrue(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ true));
    }

    @Test
    public void testShouldRazeStoreForWindow_UnknownShadow() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.UNKNOWN);
        assertTrue(mManager.shouldRazeStoreForWindow(/* isAuthoritative= */ false));
    }

    @Test
    public void testGetShadowStoreType_UnknownShadow() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.LEGACY);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.UNKNOWN);
        // Should be treated as not having a valid shadow store caught up.
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testMarkAllKeysUnknownForPrefix_DoesNotAffectOtherPrefixes() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        mManager.onAllStoresRazed();
        assertEquals(
                StoreType.UNKNOWN,
                ChromeSharedPreferences.getInstance()
                        .readInt(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.INVALID));
    }

    @Test
    public void testVersionUpgradeClearsPrefs() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(TAB_PERSISTENCE_STORE_MANAGER_VERSION, 0);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertFalse(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_1));
        assertFalse(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
        assertEquals(
                MANAGER_VERSION,
                ChromeSharedPreferences.getInstance()
                        .readInt(TAB_PERSISTENCE_STORE_MANAGER_VERSION, 0));
    }

    @Test
    public void testVersionNotUpgradedDoesNotClearPrefs() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(TAB_PERSISTENCE_STORE_MANAGER_VERSION, MANAGER_VERSION);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertTrue(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_1));
        assertTrue(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
    }
}
