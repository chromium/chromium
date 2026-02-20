// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_SHADOW_WRITTEN_STORE;

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
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(false);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(false);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKeySync(CURRENT_AUTHORITATIVE_STORE_KEY_1);
        ChromeSharedPreferences.getInstance().removeKeySync(SHADOW_WRITTEN_STORE_KEY_1);
        ChromeSharedPreferences.getInstance().removeKeySync(SHADOW_WRITTEN_STORE_KEY_2);
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
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);

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
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);

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
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    public void testShadowState_TabStateStoreToLegacy() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
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
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        // Restart with authoritative read source DISABLED (revert to Legacy)
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(false);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
    }

    @Test
    public void testMigration_Rollback() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        // Rollback to Legacy.
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(false);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());

        // Rollback to TabStateStore.
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testMigration_ShadowStoreRazedWhenFeatureDisabled() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(SHADOW_WRITTEN_STORE_KEY_1, StoreType.LEGACY);

        // Rollback to Legacy with no shadow.
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(false);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);

        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());

        // Shadow store caught up status should be reset.
        assertFalse(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
    }

    @Test
    public void testOnShadowStoreRazed() {
        mManager.onShadowStoreCreated(StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCaughtUp();
        assertTrue(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));

        mManager.onShadowStoreRazed();

        assertFalse(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
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

        assertTrue(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
        assertTrue(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_2));
        assertTrue(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_2));

        mManager.onAllStoresRazed();

        assertFalse(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
        assertFalse(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_2));
        assertFalse(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_2));
    }

    @Test
    public void testOnWindowCleared() {
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);
        mManager.onShadowStoreCreated(StoreType.LEGACY);
        mManager.onShadowStoreCaughtUp();

        assertTrue(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_1));
        assertTrue(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));

        mManager.onWindowCleared();
        assertFalse(
                ChromeSharedPreferences.getInstance().contains(CURRENT_AUTHORITATIVE_STORE_KEY_1));
        assertFalse(ChromeSharedPreferences.getInstance().contains(SHADOW_WRITTEN_STORE_KEY_1));
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
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
    }

    @Test
    public void testLegacyShadowAfterFullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(false);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
    }

    @Test
    public void testMigration_RollbackFromFullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        // Start Rollback. Start shadowing as legacy.
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(false);
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
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testMigration_ForceFullRollback() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeIntSync(CURRENT_AUTHORITATIVE_STORE_KEY_1, StoreType.TAB_STATE_STORE);

        // Should fully rollback to Legacy without catchup phase.
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(false);
        mManager = new PersistentStoreMigrationManagerImpl(WINDOW_TAG_1);
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
    }

    @Test
    public void testDefaultStore_TabStateStorageEnabled() {
        assertEquals(StoreType.LEGACY, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultStore_AuthoritativeReadsEnabled() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.LEGACY, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }

    @Test
    public void testDefaultStore_FullMigration() {
        ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.setForTesting(true);
        ChromeFeatureList.sTabStorageSqlitePrototypeAllowFullMigration.setForTesting(true);
        assertEquals(StoreType.TAB_STATE_STORE, mManager.getAuthoritativeStoreType());
        assertEquals(StoreType.INVALID, mManager.getShadowStoreType());
        assertFalse(mManager.isShadowStoreCaughtUp());
    }
}
