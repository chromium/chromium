// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

/**
 * Implementers are glue-level objects that manage lifetime of root .tabmodel objects: {@link
 * TabPersistentStore} and {@link TabModelSelectorImpl}.
 */
public abstract class TabModelOrchestrator {
    protected TabPersistentStore mTabPersistentStore;
    @Nullable protected TabModelSelectorBase mTabModelSelector;
    protected TabPersistencePolicy mTabPersistencePolicy;
    private boolean mTabModelsInitialized;
    private Callback<String> mOnStandardActiveIndexRead;
    private boolean mTabPersistentStoreDestroyedEarly;

    // TabModelStartupInfo variables
    private ObservableSupplierImpl<TabModelStartupInfo> mTabModelStartupInfoSupplier;
    private boolean mIgnoreIncognitoFiles;
    private int mStandardCount;
    private int mIncognitoCount;
    private int mStandardActiveIndex = TabModel.INVALID_TAB_INDEX;
    private int mIncognitoActiveIndex = TabModel.INVALID_TAB_INDEX;

    /**
     * @return Whether the tab models have been fully initialized.
     */
    public boolean areTabModelsInitialized() {
        return mTabModelsInitialized;
    }

    /**
     * @return The {@link TabModelSelector} managed by this orchestrator.
     */
    public TabModelSelectorBase getTabModelSelector() {
        return mTabModelSelector;
    }

    /**
     * Sets {@link TabPersistentStore} for testing.
     * @param tabPersistentStore The {@link TabPersistentStore}.
     */
    void setTabPersistentStoreForTesting(TabPersistentStore tabPersistentStore) {
        mTabPersistentStore = tabPersistentStore;
    }

    /**
     * @return The {@link TabPersistentStore} managed by this orchestrator.
     */
    public TabPersistentStore getTabPersistentStore() {
        return mTabPersistentStore;
    }

    /** Destroy the {@link TabPersistentStore} and {@link TabModelSelectorImpl} members. */
    public void destroy() {
        if (!mTabModelsInitialized) {
            return;
        }

        // TODO(crbug.com/40743848): Set the members to null and mTabModelsInitialized to false.
        // Right now, it breaks destruction of VrShell, which relies on using TabModel after
        // its destruction.

        if (mTabPersistentStore != null) {
            mTabPersistentStore.destroy();
        }

        if (mTabModelSelector != null) {
            mTabModelSelector.destroy();
        }
    }

    /**
     * Destroy the {@link TabPersistentStore} earlier than activity destruction. See the
     * implementation of {@link MismatchedIndicesHandler#handleMismatchedIndices(Activity)} for more
     * details.
     */
    public void destroyTabPersistentStore() {
        if (mTabPersistentStore != null) {
            mTabPersistentStore.destroy();
            mTabPersistentStoreDestroyedEarly = true;
        }
    }

    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        mTabModelSelector.onNativeLibraryReady(tabContentManager);
        mTabPersistencePolicy.setTabContentManager(tabContentManager);
        if (!mTabPersistentStoreDestroyedEarly) mTabPersistentStore.onNativeLibraryReady();
    }

    /**
     * Save the current state of the tab model. Usage of this method is discouraged due to it
     * writing to disk.
     */
    public void saveState() {
        mTabModelSelector.commitAllTabClosures();
        if (!mTabPersistentStoreDestroyedEarly) mTabPersistentStore.saveState();
    }

    /**
     * Load the saved tab state. This should be called before any new tabs are created. The saved
     * tabs shall not be restored until {@link #restoreTabs} is called.
     * @param ignoreIncognitoFiles Whether to skip loading incognito tabs.
     * @param onStandardActiveIndexRead The callback to be called when the active non-incognito Tab
     *                                  is found.
     */
    public void loadState(
            boolean ignoreIncognitoFiles, Callback<String> onStandardActiveIndexRead) {
        mIgnoreIncognitoFiles = ignoreIncognitoFiles;
        mOnStandardActiveIndexRead = onStandardActiveIndexRead;
        if (!mTabPersistentStoreDestroyedEarly) mTabPersistentStore.loadState(ignoreIncognitoFiles);
    }

    /**
     * Restore the saved tabs which were loaded by {@link #loadState}.
     *
     * @param setActiveTab If true, synchronously load saved active tab and set it as the current
     *                     active tab.
     */
    public void restoreTabs(boolean setActiveTab) {
        if (mTabModelStartupInfoSupplier != null) {
            boolean createdStandardTabOnStartup =
                    getTabModelSelector().getModel(false).getCount() > 0;
            boolean createdIncognitoTabOnStartup =
                    getTabModelSelector().getModel(true).getCount() > 0;

            // Incognito tabs are read first, so we have to adjust to find the real active index in
            // the standard model.
            int standardActiveIndex =
                    mStandardActiveIndex != TabModel.INVALID_TAB_INDEX
                            ? mStandardActiveIndex - mIncognitoCount
                            : TabModel.INVALID_TAB_INDEX;

            // If we're going to cull the Incognito tabs, reset the startup state.
            if (mIgnoreIncognitoFiles) {
                mIncognitoCount = 0;
                mIncognitoActiveIndex = TabModel.INVALID_TAB_INDEX;
            }

            // Account for tabs created on startup (e.g. through intents).
            if (createdStandardTabOnStartup) mStandardCount++;
            if (createdIncognitoTabOnStartup) mIncognitoCount++;

            mTabModelStartupInfoSupplier.set(
                    new TabModelStartupInfo(
                            mStandardCount,
                            mIncognitoCount,
                            standardActiveIndex,
                            mIncognitoActiveIndex,
                            createdStandardTabOnStartup,
                            createdIncognitoTabOnStartup));
        }
        if (!mTabPersistentStoreDestroyedEarly) mTabPersistentStore.restoreTabs(setActiveTab);
    }

    public void mergeState() {
        if (!mTabPersistentStoreDestroyedEarly) mTabPersistentStore.mergeState();
    }

    public void clearState() {
        if (!mTabPersistentStoreDestroyedEarly) mTabPersistentStore.clearState();
    }

    /**
     * Clean up persistent state for a given instance.
     * @param instanceId Instance ID.
     */
    public void cleanupInstance(int instanceId) {}

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore
     * the state of a tab with the given url as a frozen tab. This method has no effect if
     * there isn't a tab being restored with this url, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForUrl(String url) {
        if (mTabModelSelector.isSessionRestoreInProgress() && !mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.restoreTabStateForUrl(url);
        }
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore
     * the state of a tab with the given id as a frozen tab. This method has no effect if
     * there isn't a tab being restored with this id, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForId(int id) {
        if (mTabModelSelector.isSessionRestoreInProgress() && !mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.restoreTabStateForId(id);
        }
    }

    /**
     * @return Number of restored tabs on cold startup.
     */
    public int getRestoredTabCount() {
        if (mTabPersistentStore == null || mTabPersistentStoreDestroyedEarly) return 0;
        return mTabPersistentStore.getRestoredTabCount();
    }

    /**
     * Sets the supplier for {@link TabModelStartupInfo} on startup.
     *
     * @param observableSupplier The {@link TabModelStartupInfo} supplier.
     */
    public void setStartupInfoObservableSupplier(
            ObservableSupplierImpl<TabModelStartupInfo> observableSupplier) {
        mTabModelStartupInfoSupplier = observableSupplier;
    }

    public boolean getTabPersistentStoreDestroyedEarlyForTesting() {
        return mTabPersistentStoreDestroyedEarly;
    }

    protected void wireSelectorAndStore() {
        if (mTabPersistentStoreDestroyedEarly) return;
        // Notify TabModelSelector when TabPersistentStore initializes tab state
        final TabPersistentStoreObserver persistentStoreObserver =
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        mTabModelSelector.markTabStateInitialized();
                    }

                    @Override
                    public void onDetailsRead(
                            int index,
                            int id,
                            String url,
                            boolean isStandardActiveIndex,
                            boolean isIncognitoActiveIndex,
                            Boolean isIncognito,
                            boolean fromMerge) {
                        if (isIncognito == null || !isIncognito.booleanValue()) {
                            mStandardCount++;
                        } else {
                            mIncognitoCount++;
                        }

                        // We prioritize focusing the active tab from the "primary" (non-merging)
                        // instance.
                        if (!fromMerge) {
                            if (isStandardActiveIndex) {
                                mStandardActiveIndex = index;
                            } else if (isIncognitoActiveIndex) {
                                mIncognitoActiveIndex = index;
                            }
                        }

                        if (mOnStandardActiveIndexRead != null && isStandardActiveIndex) {
                            mOnStandardActiveIndexRead.onResult(url);
                        }
                    }

                    @Override
                    public void onInitialized(int tabCountAtStartup) {
                        // Resets the callback once the read of the Tab state file is completed.
                        mOnStandardActiveIndexRead = null;
                    }
                };
        mTabPersistentStore.addObserver(persistentStoreObserver);
    }

    protected void markTabModelsInitialized() {
        mTabModelsInitialized = true;
    }
}
