// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
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
     * @param profileProviderSupplier The supplier for profiles.
     * @param regularShadowTabCreator The accumulator for regular tabs loaded by the shadow store.
     * @param incognitoShadowTabCreator The accumulator for incognito tabs loaded by the shadow
     *     store.
     * @param selector The selector associated with the store.
     * @param tabPersistencePolicy The tab persistence to use for the shadow store.
     * @param authoritativeStore The primary {@link TabPersistentStore} that acts as the source of
     *     truth.
     * @param windowTag The unique identifier for the window instance.
     * @param cipherFactory The cipher factory to use for encryption to the store.
     * @param recordMetrics Whether to record metrics for the shadow store;
     */
    public static @Nullable TabPersistentStore buildShadowStore(
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            AccumulatingTabCreator regularShadowTabCreator,
            AccumulatingTabCreator incognitoShadowTabCreator,
            TabModelSelector selector,
            TabPersistencePolicy tabPersistencePolicy,
            TabPersistentStore authoritativeStore,
            String windowTag,
            CipherFactory cipherFactory,
            boolean recordMetrics) {
        if (!TabStateStorageFlagHelper.isTabStorageEnabled()) return null;

        assert profileProviderSupplier.get() != null;
        ProfileProvider profileProvider = profileProviderSupplier.get();
        Profile profile = profileProvider.getOriginalProfile();
        assert profile != null;

        TabCreatorManager shadowTabCreatorManager =
                incognito -> incognito ? incognitoShadowTabCreator : regularShadowTabCreator;

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
                recordMetrics);
        return shadowTabPersistentStore;
    }
}
