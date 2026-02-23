// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isStorageAuthoritative;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isTabStorageEnabled;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
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
    // A migration manager which always assumes no migration is required.
    private static final PersistentStoreMigrationManager sDefaultManager =
            new PersistentStoreMigrationManager() {
                @Override
                public @StoreType int getAuthoritativeStoreType() {
                    return (isTabStorageEnabled() && isStorageAuthoritative())
                            ? StoreType.TAB_STATE_STORE
                            : StoreType.LEGACY;
                }

                @Override
                public @StoreType int getShadowStoreType() {
                    return (isTabStorageEnabled() && !isStorageAuthoritative())
                            ? StoreType.TAB_STATE_STORE
                            : StoreType.INVALID;
                }

                @Override
                public void onShadowStoreCreated(@StoreType int storeType) {}

                @Override
                public void onShadowStoreCaughtUp() {}

                @Override
                public boolean isShadowStoreCaughtUp() {
                    return true;
                }

                @Override
                public void onShadowStoreRazed() {}

                @Override
                public void onAllStoresRazed() {}

                @Override
                public void onWindowCleared() {}
            };

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
            boolean recordLegacyTabCountMetrics) {
        if (migrationManager == null) migrationManager = sDefaultManager;

        @StoreType int storeType = migrationManager.getAuthoritativeStoreType();
        if (storeType == StoreType.LEGACY) {
            return new TabPersistentStoreImpl(
                    clientTag,
                    tabPersistencePolicy,
                    tabModelSelector,
                    tabCreatorManager,
                    tabWindowManager,
                    cipherFactory,
                    recordLegacyTabCountMetrics);
        } else if (storeType == StoreType.TAB_STATE_STORE) {
            assert isTabStorageEnabled();
            assert isStorageAuthoritative();
            return new TabStateStore(
                    tabModelSelector,
                    windowTag,
                    tabCreatorManager,
                    tabPersistencePolicy,
                    migrationManager,
                    cipherFactory,
                    new TabCountTracker(windowTag),
                    ModelTrackingOrchestrator::new,
                    /* isAuthoritative= */ true);
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
     */
    public static @Nullable TabPersistentStore buildShadowStore(
            @Nullable PersistentStoreMigrationManager migrationManager,
            AccumulatingTabCreator regularShadowTabCreator,
            AccumulatingTabCreator incognitoShadowTabCreator,
            TabModelSelector selector,
            TabPersistencePolicy tabPersistencePolicy,
            TabPersistentStore authoritativeStore,
            String windowTag,
            @Nullable CipherFactory cipherFactory,
            String orchestratorTag) {
        TabCreatorManager shadowTabCreatorManager =
                incognito -> incognito ? incognitoShadowTabCreator : regularShadowTabCreator;

        return buildShadowStoreInternal(
                migrationManager,
                shadowTabCreatorManager,
                selector,
                tabPersistencePolicy,
                authoritativeStore,
                windowTag,
                cipherFactory,
                regularShadowTabCreator,
                orchestratorTag);
    }

    /**
     * Builds a shadow {@link TabPersistentStore} for validation against an authoritative store.
     * This store will only function with non-OTR data. Returns null if a shadow store is not
     * enabled via feature flags.
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
     * @param selector The selector associated with the store.
     * @param tabPersistencePolicy The tab persistence to use for the shadow store.
     * @param authoritativeStore The primary {@link TabPersistentStore} that acts as the source of
     *     truth.
     * @param windowTag The unique identifier for the window instance.
     * @param orchestratorTag A tag representing the type of tab model orchestrator this validator
     *     is for.
     */
    public static @Nullable TabPersistentStore buildNonOtrShadowStore(
            @Nullable PersistentStoreMigrationManager migrationManager,
            AccumulatingTabCreator regularShadowTabCreator,
            TabModelSelector selector,
            TabPersistencePolicy tabPersistencePolicy,
            TabPersistentStore authoritativeStore,
            String windowTag,
            String orchestratorTag) {
        TabCreatorManager shadowTabCreatorManager =
                incognito -> {
                    assert !incognito;
                    return regularShadowTabCreator;
                };

        return buildShadowStoreInternal(
                migrationManager,
                shadowTabCreatorManager,
                selector,
                tabPersistencePolicy,
                authoritativeStore,
                windowTag,
                /* cipherFactory= */ null,
                regularShadowTabCreator,
                orchestratorTag);
    }

    private static @Nullable TabPersistentStore buildShadowStoreInternal(
            @Nullable PersistentStoreMigrationManager migrationManager,
            TabCreatorManager shadowTabCreatorManager,
            TabModelSelector selector,
            TabPersistencePolicy tabPersistencePolicy,
            TabPersistentStore authoritativeStore,
            String windowTag,
            @Nullable CipherFactory cipherFactory,
            AccumulatingTabCreator regularShadowTabCreator,
            String orchestratorTag) {
        if (migrationManager == null) migrationManager = sDefaultManager;
        @StoreType int shadowStoreType = migrationManager.getShadowStoreType();
        if (shadowStoreType != StoreType.TAB_STATE_STORE) return null;
        assert isTabStorageEnabled();

        TabPersistentStore shadowTabPersistentStore =
                new TabStateStore(
                        selector,
                        windowTag,
                        shadowTabCreatorManager,
                        tabPersistencePolicy,
                        migrationManager,
                        cipherFactory,
                        new TabCountTracker(windowTag),
                        ModelTrackingOrchestrator::new,
                        /* isAuthoritative= */ false);

        new ShadowTabStoreValidator(
                authoritativeStore,
                shadowTabPersistentStore,
                selector.getModel(/* incognito= */ false),
                regularShadowTabCreator,
                migrationManager,
                orchestratorTag);

        migrationManager.onShadowStoreCreated(shadowStoreType);
        return shadowTabPersistentStore;
    }
}
