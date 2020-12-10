// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

/**
 * Implementers are glue-level objects that manage lifetime of root .tabmodel objects: {@link
 * TabPersistentStore} and {@link TabModelSelectorImpl}.
 */
public abstract class TabModelOrchestrator {
    protected TabPersistentStore mTabPersistentStore;
    protected TabModelSelectorImpl mTabModelSelector;
    private boolean mTabModelsInitialized;

    /**
     * @return Whether the tab models have been fully initialized.
     */
    public boolean areTabModelsInitialized() {
        return mTabModelsInitialized;
    }

    /**
     * @return The {@link TabModelSelectorImpl} managed by this orchestrator.
     */
    public TabModelSelectorImpl getTabModelSelector() {
        return mTabModelSelector;
    }

    /**
     * Destroy the {@link TabPersistentStore} and {@link TabModelSelectorImpl} members.
     */
    public void destroy() {
        if (!mTabModelsInitialized) {
            return;
        }

        if (mTabPersistentStore != null) {
            mTabPersistentStore.destroy();
            mTabPersistentStore = null;
        }

        if (mTabModelSelector != null) {
            mTabModelSelector.destroy();
            mTabModelSelector = null;
        }

        mTabModelsInitialized = false;
    }

    protected void wireSelectorAndStore() {
        // Supply TabModelSelectorImpl with TabPersistentStore.
        //
        // TODO(crbug.com/1138561): Remove this dependency by making TabModelSelectorImpl emit
        // events and TabPersistentStore react to them as an observer.
        ObservableSupplierImpl<TabPersistentStore> tabPersistentStoreSupplier =
                new ObservableSupplierImpl<>();
        tabPersistentStoreSupplier.set(mTabPersistentStore);
        mTabModelSelector.setTabPersistentStoreSupplier(tabPersistentStoreSupplier);

        // Notify TabModelSelectorImpl when TabPersistentStore initializes tab state
        final TabPersistentStoreObserver persistentStoreObserver =
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        mTabModelSelector.markTabStateInitialized();
                    }
                };
        mTabPersistentStore.addObserver(persistentStoreObserver);
    }

    protected void markTabModelsInitialized() {
        mTabModelsInitialized = true;
    }
}
