// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isTabStorageEnabled;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreator;
import org.chromium.chrome.browser.tabmodel.RecordingTabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/**
 * Factory class for creating instances of {@link TabPersistentStore}.
 *
 * <p>This factory encapsulates the logic for creating different variations of stores, including
 * shadow stores and authoritative stores.
 *
 * <p>The {@link TabPersistentStore}s returned from this class are uninitialized, and {@link
 * TabPersistentStore#onNativeLibraryReady()} must be called to initialize them.
 */
@NullMarked
public class TabPersistentStoreFactory {
    /**
     * Builds an authoritative {@link TabPersistentStore}.
     *
     * <p>This store acts as the source of truth for tab state. It directly manages reading from and
     * writing to the disk for the associated {@link TabModelSelector}.
     *
     * @param clientTag The client tag used to record metrics for this specific store instance.
     * @param migrationManager Determines which implementation of {@link TabPersistentStore} to
     *     return. If null, use fallback logic.
     * @param tabPersistencePolicy The {@link TabPersistencePolicy} to use for the window.
     * @param tabModelSelector The selector to observe and manage persistence for.
     * @param tabCreatorManager Used to create new tabs during state restoration.
     * @param tabWindowManager Used to coordinate tab state across multiple windows.
     * @param windowTag The unique identifier for the window instance.
     * @param cipherFactory Used for encrypting and decrypting tab state files.
     * @param recordLegacyTabCountMetrics Whether to record legacy metrics regarding tab counts.
     * @param isFromRecreating Whether the current activity is launched from recreating.
     */
    public static TabPersistentStore buildAuthoritativeStore(
            String clientTag,
            @Nullable PersistentStoreMigrationManager migrationManager,
            TabPersistencePolicy tabPersistencePolicy,
            TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager,
            TabWindowManager tabWindowManager,
            String windowTag,
            CipherFactory cipherFactory,
            boolean recordLegacyTabCountMetrics,
            boolean isFromRecreating) {
        if (migrationManager == null) {
            migrationManager = new DefaultPersistentStoreMigrationManager(windowTag);
        }

        @StoreType int storeType = migrationManager.getAuthoritativeStoreType();
        recordStoreTypeHistogram("Tabs.TabPersistentStore.AuthoritativeStoreType", storeType);
        if (storeType == StoreType.LEGACY) {
            TabPersistentStoreImpl legacyStore =
                    new TabPersistentStoreImpl(
                            clientTag,
                            tabPersistencePolicy,
                            tabModelSelector,
                            tabCreatorManager,
                            tabWindowManager,
                            cipherFactory,
                            /* isAuthoritative= */ true,
                            recordLegacyTabCountMetrics);
            buildAuthoritativeLegacyStoreInitTracker(legacyStore, migrationManager);
            return legacyStore;
        } else if (storeType == StoreType.TAB_STATE_STORE) {
            assert isTabStorageEnabled();
            return new TabStateStore(
                    tabModelSelector,
                    windowTag,
                    tabCreatorManager,
                    tabPersistencePolicy,
                    migrationManager,
                    cipherFactory,
                    new TabCountTracker(windowTag),
                    ModelTrackingOrchestrator::new,
                    ActiveTabCache::new,
                    /* isAuthoritative= */ true,
                    isFromRecreating);
        }
        throw new IllegalStateException();
    }

    /**
     * Builds a shadow {@link TabPersistentStore} for validation against an authoritative store.
     * Returns null if a shadow store is not enabled via feature flags.
     *
     * <p>This method creates a {@link TabStateStore} that operates in "shadow" mode. It captures
     * the state of a selector without affecting the authoritative application state.
     *
     * <p>Shadow stores may only be initialized post-native. This is due to feature flags only being
     * updated post-native, which may result in stale flag data being used to determine which
     * implementation to use.
     *
     * @param migrationManager Determines which implementation of {@link TabPersistentStore} to
     *     return. If null, use fallback logic.
     * @param regularShadowTabCreator The accumulator for regular tabs loaded by the shadow store.
     * @param incognitoShadowTabCreator The accumulator for incognito tabs loaded by the shadow
     *     store.
     * @param selector The selector associated with the store.
     * @param tabPersistencePolicy The tab persistence to use for the shadow store.
     * @param authoritativeStore The primary {@link TabPersistentStore} that acts as the source of
     *     truth.
     * @param windowTag The unique identifier for the window instance.
     * @param cipherFactory The cipher factory to use for encryption to the store.
     * @param orchestratorTag A tag representing the type of tab model orchestrator this validator
     *     is for.
     * @param isNonOtrOnly Whether the shadow store should only handle non-OTR data. If true,
     *     incognito tab state will not be restored and may be destroyed if a TabStateStore is
     *     returned.
     * @param isFromRecreating Whether the current activity was recreated.
     */
    public static @Nullable TabPersistentStore buildShadowStore(
            @Nullable PersistentStoreMigrationManager migrationManager,
            AccumulatingTabCreator regularShadowTabCreator,
            AccumulatingTabCreator incognitoShadowTabCreator,
            TabModelSelector selector,
            RecordingTabCreatorManager recordingTabCreatorManager,
            TabPersistencePolicy tabPersistencePolicy,
            TabPersistentStore authoritativeStore,
            String windowTag,
            CipherFactory cipherFactory,
            String orchestratorTag,
            boolean isNonOtrOnly,
            boolean isFromRecreating) {
        TabCreatorManager shadowTabCreatorManager =
                incognito -> incognito ? incognitoShadowTabCreator : regularShadowTabCreator;

        if (incognitoShadowTabCreator != null) {
            incognitoShadowTabCreator.stopRecording();
        }

        if (migrationManager == null) {
            migrationManager = new DefaultPersistentStoreMigrationManager(windowTag);
        }

        @StoreType int shadowStoreType = migrationManager.getShadowStoreType();
        recordStoreTypeHistogram("Tabs.TabPersistentStore.ShadowStoreType", shadowStoreType);
        TabPersistentStore shadowTabPersistentStore;
        if (shadowStoreType == StoreType.TAB_STATE_STORE) {
            // Headless and Archived models specifically choose not to restore incognito tabs
            // during Legacy to TabStateStore migrations by withholding passing a cipher factory to
            // the shadow store. In this case, the shadow store will delete incognito tab state
            // during migrations.
            assert isTabStorageEnabled();
            TabStateStore tabStateStore =
                    new TabStateStore(
                            selector,
                            windowTag,
                            shadowTabCreatorManager,
                            tabPersistencePolicy,
                            migrationManager,
                            isNonOtrOnly ? null : cipherFactory,
                            new TabCountTracker(windowTag),
                            ModelTrackingOrchestrator::new,
                            ActiveTabCache::new,
                            /* isAuthoritative= */ false,
                            isFromRecreating);
            buildShadowTabStateStoreCatchupTracker(authoritativeStore, tabStateStore);
            shadowTabPersistentStore = tabStateStore;
        } else if (shadowStoreType == StoreType.LEGACY) {
            shadowTabPersistentStore =
                    new TabPersistentStoreImpl(
                            orchestratorTag,
                            tabPersistencePolicy,
                            selector,
                            shadowTabCreatorManager,
                            TabWindowManagerSingleton.getInstance(),
                            cipherFactory,
                            /* isAuthoritative= */ false,
                            /* recordLegacyTabCountMetrics= */ false);
            buildShadowLegacyStoreCatchupTracker(authoritativeStore, migrationManager);
        } else {
            regularShadowTabCreator.stopRecording();
            RecordingTabCreator recordingTabCreator =
                    recordingTabCreatorManager.getRecorder(/* incognito= */ false);
            if (recordingTabCreator != null) {
                recordingTabCreator.stopRecording();
            }
            return null;
        }

        RecordingTabCreator recordingTabCreator =
                recordingTabCreatorManager.getRecorder(/* incognito= */ false);
        assert recordingTabCreator != null;

        new ShadowTabStoreValidator(
                authoritativeStore,
                shadowTabPersistentStore,
                recordingTabCreator,
                regularShadowTabCreator,
                migrationManager,
                orchestratorTag);

        migrationManager.onShadowStoreCreated(shadowStoreType);
        return shadowTabPersistentStore;
    }

    private static void buildAuthoritativeLegacyStoreInitTracker(
            TabPersistentStore authoritativeStore, PersistentStoreMigrationManager manager) {
        authoritativeStore.addObserver(
                new TabPersistentStoreObserver() {
                    @Override
                    public void onInitialized(int tabCountAtStartup) {
                        authoritativeStore.removeObserver(this);
                        manager.onAuthoritativeStoreInitialized(StoreType.LEGACY);
                    }
                });
    }

    private static void buildShadowLegacyStoreCatchupTracker(
            TabPersistentStore authoritativeStore, PersistentStoreMigrationManager manager) {
        authoritativeStore.addObserver(
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        authoritativeStore.removeObserver(this);
                        manager.onShadowStoreCaughtUp();
                    }
                });
    }

    private static void buildShadowTabStateStoreCatchupTracker(
            TabPersistentStore authoritativeStore, TabStateStore shadowStore) {
        authoritativeStore.addObserver(
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        authoritativeStore.removeObserver(this);
                        shadowStore.onAuthoritativeStateLoaded();
                    }
                });
    }

    private static void recordStoreTypeHistogram(String name, @StoreType int storeType) {
        RecordHistogram.recordEnumeratedHistogram(name, storeType, StoreType.UNKNOWN);
    }
}
