// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;

/**
 * Factory class for creating instances of {@link TabPersistentStore}.
 *
 * <p>This factory encapsulates the logic for creating different variations of stores, including
 * shadow stores and authoritative stores.
 */
@NullMarked
public class TabPersistentStoreFactory {

    /**
     * Builds a shadow {@link TabPersistentStore} for validation against an authoritative store.
     * Returns null if a shadow store is not enabled via feature flags.
     *
     * <p>This method creates a {@link TabStateStore} that operates in "shadow" mode. It captures
     * the state of a selector without affecting the authoritative application state.
     *
     * @param profile The original profile associated with the tabs.
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
            Profile profile,
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
                profile,
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
     * @param profile The original profile associated with the tabs.
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
            Profile profile,
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
                profile,
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
            Profile profile,
            TabCreatorManager shadowTabCreatorManager,
            TabModelSelector selector,
            TabPersistencePolicy tabPersistencePolicy,
            TabPersistentStore authoritativeStore,
            String windowTag,
            @Nullable CipherFactory cipherFactory,
            AccumulatingTabCreator regularShadowTabCreator,
            String orchestratorTag) {
        if (!TabStateStorageFlagHelper.isTabStorageEnabled()) return null;
        assert !profile.isOffTheRecord();

        TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(profile);
        assert service != null;

        TabPersistentStore shadowTabPersistentStore =
                new TabStateStore(
                        service,
                        selector,
                        windowTag,
                        shadowTabCreatorManager,
                        tabPersistencePolicy,
                        cipherFactory);

        new ShadowTabStoreValidator(
                authoritativeStore,
                shadowTabPersistentStore,
                selector.getModel(/* incognito= */ false),
                regularShadowTabCreator,
                orchestratorTag);
        return shadowTabPersistentStore;
    }
}
