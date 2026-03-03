// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.TabStateStore.TabStateStoreCleaner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl.TabPersistentStoreImplCleaner;

/** Unit tests for {@link PersistentStoreCleaner}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
public class PersistentStoreCleanerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelOrchestrator mOrchestrator;
    @Mock private TabModelSelectorBase mSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private TabPersistentStoreImplCleaner mLegacyStoreCleaner;
    @Mock private TabStateStoreCleaner mTabStateStoreCleaner;

    @Before
    public void setUp() {
        when(mOrchestrator.getTabModelSelector()).thenReturn(mSelector);
        when(mSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        PersistentStoreCleaner.setTabPersistentStoreImplCleanerForTesting(
                () -> mLegacyStoreCleaner);
        PersistentStoreCleaner.setTabStateStoreCleanerForTesting(() -> mTabStateStoreCleaner);
    }

    @Test
    public void testCleanWindow_BothStoresActive() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.LEGACY);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.TAB_STATE_STORE);

        PersistentStoreCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner, never()).cleanupStateFile(anyInt(), any(), any(), any());
        verify(mTabStateStoreCleaner, never()).cleanupStateFile(anyInt(), any());
    }

    @Test
    public void testCleanWindow_LegacyOnly() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.LEGACY);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        PersistentStoreCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner, never()).cleanupStateFile(anyInt(), any(), any(), any());
        verify(mTabStateStoreCleaner).cleanupStateFile(eq(1), eq(mProfile));
    }

    @Test
    public void testCleanWindow_TabStateStoreOnly() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.TAB_STATE_STORE);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        PersistentStoreCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner).cleanupStateFile(eq(1), any(), any(), any());
        verify(mTabStateStoreCleaner, never()).cleanupStateFile(anyInt(), any());
    }

    @Test
    public void testClearState_BothStoresActive() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.LEGACY);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.TAB_STATE_STORE);

        PersistentStoreCleaner.cleanAllWindowsForUnavailableStores(mOrchestrator);

        verify(mLegacyStoreCleaner, never()).clearState(any(), any());
        verify(mTabStateStoreCleaner, never()).clearState(any());
    }

    @Test
    public void testClearState_NeitherStoreActive() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.INVALID);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        PersistentStoreCleaner.cleanAllWindowsForUnavailableStores(mOrchestrator);

        verify(mLegacyStoreCleaner).clearState(any(), any());
        verify(mTabStateStoreCleaner).clearState(eq(mProfile));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testCleanWindow_TabStateStoreDisabled() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.INVALID);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        PersistentStoreCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner).cleanupStateFile(eq(1), any(), any(), any());
        verify(mTabStateStoreCleaner, never()).cleanupStateFile(anyInt(), any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testClearState_TabStateStoreDisabled() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.INVALID);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        PersistentStoreCleaner.cleanAllWindowsForUnavailableStores(mOrchestrator);

        verify(mLegacyStoreCleaner).clearState(any(), any());
        verify(mTabStateStoreCleaner, never()).clearState(any());
    }

    @Test
    public void testClearState_TabStateStoreOnly() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.TAB_STATE_STORE);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        PersistentStoreCleaner.cleanAllWindowsForUnavailableStores(mOrchestrator);

        verify(mLegacyStoreCleaner).clearState(any(), any());
        verify(mTabStateStoreCleaner, never()).clearState(any());
    }
}
