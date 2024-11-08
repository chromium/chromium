// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;

/**
 * Glue-level class that manages lifetime of root .tabmodel objects: {@link TabPersistentStore} and
 * {@link TabModelSelectorImpl} for custom tabs.
 */
public class CustomTabsTabModelOrchestrator extends TabModelOrchestrator {
    public CustomTabsTabModelOrchestrator() {}

    /** Creates the TabModelSelector and the TabPersistentStore. */
    public void createTabModels(
            Context context,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            TabPersistencePolicy persistencePolicy,
            @ActivityType int activityType,
            AsyncTabParamsManager asyncTabParamsManager,
            CipherFactory cipherFactory) {
        // Instantiate TabModelSelectorImpl
        NextTabPolicySupplier nextTabPolicySupplier = () -> NextTabPolicy.LOCATIONAL;
        mTabModelSelector =
                new TabModelSelectorImpl(
                        context,
                        /* modalDialogManager= */ null,
                        profileProviderSupplier,
                        tabCreatorManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        false,
                        activityType,
                        false);

        // Instantiate TabPersistentStore
        mTabPersistencePolicy = persistencePolicy;
        mTabPersistentStore =
                new TabPersistentStore(
                        TabPersistentStore.CLIENT_TAG_CUSTOM,
                        mTabPersistencePolicy,
                        mTabModelSelector,
                        tabCreatorManager,
                        TabWindowManagerSingleton.getInstance(),
                        cipherFactory);

        wireSelectorAndStore();
        markTabModelsInitialized();
    }
}
