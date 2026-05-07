// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.TimeUtils.uptimeMillis;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl;

/**
 * Implementers are glue-level objects that manage lifetime of root .tabmodel objects: {@link
 * TabPersistentStore} and {@link TabModelSelectorImpl}.
 */
@NullMarked
public class TabModelOrchestrator {
    protected final ObserverList<TabModelOrchestratorObserver> mObservers = new ObserverList<>();
    private final long mInitializationTime = uptimeMillis();

    protected @MonotonicNonNull TabPersistentStore mTabPersistentStore;
    protected @MonotonicNonNull TabModelSelectorBase mTabModelSelector;
    protected @MonotonicNonNull TabPersistencePolicy mTabPersistencePolicy;
    protected @Nullable TabPersistentStore mShadowTabPersistentStore;
    protected @Nullable PersistentStoreMigrationManager mMigrationManager;
    protected boolean mTabPersistentStoreDestroyedEarly;
    private boolean mTabModelsInitialized;
    private @Nullable Callback<String> mOnStandardActiveIndexRead;
    private boolean mIsDestroyed;
    protected boolean mStoresInitialized;

    // TabModelStartupInfo variables
    private @Nullable SettableMonotonicObservableSupplier<TabModelStartupInfo>
            mTabModelStartupInfoSupplier;
    private boolean mIgnoreIncognitoFiles;
    private boolean mIgnoreRegularFiles;
    private int mStandardCount;
    private int mIncognitoCount;
    private int mStandardActiveIndex = TabModel.INVALID_TAB_INDEX;
    private int mIncognitoActiveIndex = TabModel.INVALID_TAB_INDEX;

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public TabModelOrchestrator() {}

    @EnsuresNonNull({
        "mTabModelSelector",
        "mTabPersistencePolicy",
        "mTabPersistentStore",
    })
    private void assertInitialized() {
        assert mTabModelSelector != null;
        assert mTabPersistencePolicy != null;
        assert mTabPersistentStore != null;
    }

    /**
     * @return Whether the tab models have been fully initialized.
     */
    public boolean areTabModelsInitialized() {
        return mTabModelsInitialized;
    }

    /**
     * @return The {@link TabModelSelector} managed by this orchestrator.
     */
    public @Nullable TabModelSelectorBase getTabModelSelector() {
        return mTabModelSelector;
    }

    /**
     * @return getTabModelSelector().getCurrentTab()
     */
    public @Nullable Tab getCurrentTab() {
        return mTabModelSelector == null ? null : mTabModelSelector.getCurrentTab();
    }

    /**
     * @return The {@link TabPersistentStore} managed by this orchestrator.
     */
    public TabPersistentStore getTabPersistentStore() {
        assert mTabPersistentStore != null;
        return mTabPersistentStore;
    }

    /** Destroy the {@link TabPersistentStore} and {@link TabModelSelectorImpl} members. */
    public void destroy() {
        if (mIsDestroyed) return;
        mIsDestroyed = true;

        if (mShadowTabPersistentStore != null) {
            mShadowTabPersistentStore.destroy();
            mShadowTabPersistentStore = null;
        }

        if (mTabPersistentStore != null && !mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.destroy();
        }

        if (mTabModelSelector != null) {
            mTabModelSelector.destroy();
        }

        mTabModelsInitialized = false;
    }

    /**
     * Destroy the {@link TabPersistentStore} earlier than activity destruction. See the
     * implementation of {@link MismatchedIndicesHandler#handleMismatchedIndices(Activity)} for more
     * details.
     */
    public void destroyTabPersistentStore() {
        if (mShadowTabPersistentStore != null) {
            mShadowTabPersistentStore.destroy();
            mShadowTabPersistentStore = null;
        }

        if (mTabPersistentStore != null) {
            mTabPersistentStore.destroy();
            mTabPersistentStoreDestroyedEarly = true;
        }
    }

    public void onNativeLibraryReady(TabContentManager tabContentManager) {
        assertInitialized();
        mTabModelSelector.onNativeLibraryReady(tabContentManager);
        mTabPersistencePolicy.setTabContentManager(tabContentManager);

        if (!mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.onNativeLibraryReady();
        }
    }

    /**
     * Save the current state of the tab model. Usage of this method is discouraged due to it
     * writing to disk.
     */
    public void saveState() {
        assertInitialized();
        mTabModelSelector.commitAllTabClosures();
        if (!mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.saveState();
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.saveState();
            }
        }
    }

    /**
     * Load the saved tab state. This should be called before any new tabs are created. The saved
     * tabs shall not be restored until {@link #restoreTabs} is called.
     *
     * @param ignoreIncognitoFiles Whether to skip loading incognito tabs.
     * @param ignoreRegularFiles Whether to skip loading regular tabs.
     * @param onStandardActiveIndexRead The callback to be called when the active non-incognito Tab
     *     is found.
     */
    public void loadState(
            boolean ignoreIncognitoFiles,
            boolean ignoreRegularFiles,
            @Nullable Callback<String> onStandardActiveIndexRead) {
        assertInitialized();
        mIgnoreIncognitoFiles = ignoreIncognitoFiles;
        mIgnoreRegularFiles = ignoreRegularFiles;
        mOnStandardActiveIndexRead = onStandardActiveIndexRead;

        if (!mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.loadState(ignoreIncognitoFiles, ignoreRegularFiles);
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.loadState(ignoreIncognitoFiles, ignoreRegularFiles);
            }
        }
    }

    /**
     * Restore the saved tabs which were loaded by {@link #loadState}.
     *
     * @param setActiveTab If true, synchronously load saved active tab and set it as the current
     *     active tab.
     */
    public void restoreTabs(boolean setActiveTab) {
        assertInitialized();
        if (mTabModelStartupInfoSupplier != null) {
            assert mTabModelSelector != null;
            boolean createdStandardTabOnStartup = mTabModelSelector.getModel(false).getCount() > 0;
            boolean createdIncognitoTabOnStartup = mTabModelSelector.getModel(true).getCount() > 0;

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

            // If we're going to cull the regular tabs, reset the startup state.
            if (mIgnoreRegularFiles) {
                mStandardCount = 0;
                standardActiveIndex = TabModel.INVALID_TAB_INDEX;
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
        if (!mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.restoreTabs(setActiveTab);
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.restoreTabs(setActiveTab);
            }
        }
    }

    public void mergeState() {
        assertInitialized();
        if (!mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.mergeState();
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.mergeState();
            }
        }
    }

    public void clearState() {
        assertInitialized();
        if (!mTabPersistentStoreDestroyedEarly) {
            assert mStoresInitialized;
            mTabPersistentStore.clearState();
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.clearState();
            }
            if (mMigrationManager != null) mMigrationManager.onAllStoresRazed();
            if (mTabModelSelector != null) {
                Profile profile = mTabModelSelector.getProfile(/* offTheRecord= */ false);
                if (profile != null) {
                    PersistentStoreCleanerFactory.getForProfile(profile).clearState(this);
                }
            }
        }
    }

    /**
     * Clean up persistent state for a given instance.
     *
     * @param instanceId Instance ID.
     */
    public void cleanupInstance(int instanceId) {
        String windowTag = String.valueOf(instanceId);
        PersistentStoreMigrationManager migrationManager =
                new PersistentStoreMigrationManagerImpl(windowTag);
        migrationManager.onWindowCleared();
    }

    /** Clean up persisted state for this orchestrator. */
    public void clearCurrentWindow() {
        assertInitialized();
        if (!mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.clearCurrentWindow();
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.clearCurrentWindow();
            }
            if (mMigrationManager != null) mMigrationManager.onWindowCleared();
        }
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore the
     * state of a tab with the given url as a frozen tab. This method has no effect if there isn't a
     * tab being restored with this url, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForUrl(String url) {
        assertInitialized();
        if (!mTabModelSelector.isTabStateInitialized() && !mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.restoreTabStateForUrl(url);
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.restoreTabStateForUrl(url);
            }
        }
    }

    /**
     * If there is an asynchronous session restore in-progress, try to synchronously restore the
     * state of a tab with the given id as a frozen tab. This method has no effect if there isn't a
     * tab being restored with this id, or the tab has already been restored.
     */
    public void tryToRestoreTabStateForId(int id) {
        assertInitialized();
        if (!mTabModelSelector.isTabStateInitialized() && !mTabPersistentStoreDestroyedEarly) {
            mTabPersistentStore.restoreTabStateForId(id);
            if (mShadowTabPersistentStore != null) {
                mShadowTabPersistentStore.restoreTabStateForId(id);
            }
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
            @Nullable SettableMonotonicObservableSupplier<TabModelStartupInfo> observableSupplier) {
        mTabModelStartupInfoSupplier = observableSupplier;
    }

    /**
     * Adds an observer for {@link TabModelOrchestrator} changes.
     *
     * @param observer The observer to add.
     */
    public void addObserver(TabModelOrchestratorObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer for {@link TabModelOrchestrator} changes.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(TabModelOrchestratorObserver observer) {
        mObservers.removeObserver(observer);
    }

    public boolean getTabPersistentStoreDestroyedEarlyForTesting() {
        return mTabPersistentStoreDestroyedEarly;
    }

    /**
     * Returns the {@link StoreType} for the authoritative store or null if the store does not exist
     * yet.
     */
    public @Nullable @StoreType Integer getAuthoritativeStoreType() {
        return mTabPersistentStore != null ? mTabPersistentStore.getStoreType() : null;
    }

    /**
     * Returns the {@link StoreType} for the authoritative store or null if the store does not exist
     * yet.
     */
    public @Nullable @StoreType Integer getShadowStoreType() {
        return mShadowTabPersistentStore != null ? mShadowTabPersistentStore.getStoreType() : null;
    }

    /** Whether the stores have been initialized. */
    public boolean areStoresInitialized() {
        return mStoresInitialized;
    }

    protected void wireSelectorAndStore() {
        if (mTabPersistentStoreDestroyedEarly) return;
        assertInitialized();
        // Notify TabModelSelector when TabPersistentStore initializes tab state
        mTabPersistentStore.addObserver(
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        mTabModelSelector.markTabStateInitialized();
                        recordTimeHistogram("Tabs.TabPersistentStore.StateLoaded");
                    }

                    @Override
                    public void onDetailsRead(
                            int index,
                            int id,
                            String url,
                            boolean isStandardActiveIndex,
                            boolean isIncognitoActiveIndex,
                            @Nullable Boolean isIncognito,
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
                    public void onActiveTabLoaded(boolean incognito) {
                        recordTimeHistogram(
                                "Tabs.TabPersistentStore.ActiveTabLoaded."
                                        + (incognito ? "Incognito" : "Regular"));
                    }

                    @Override
                    public void onInitialized(int tabCountAtStartup) {
                        // Resets the callback once the read of the Tab state file is completed.
                        mOnStandardActiveIndexRead = null;
                        recordTimeHistogram("Tabs.TabPersistentStore.Initialized");
                    }
                });
    }

    protected void markTabModelsInitialized() {
        if (mIsDestroyed) return;
        mTabModelsInitialized = true;
    }

    protected void markStoresInitialized() {
        if (mIsDestroyed) return;
        mStoresInitialized = true;

        for (TabModelOrchestratorObserver observer : mObservers) {
            observer.onStoresInitialized();
        }
    }

    /**
     * Sets {@link TabPersistentStoreImpl} for testing.
     *
     * @param tabPersistentStore The {@link TabPersistentStoreImpl}.
     * @param shadowPersistentStore The shadow {@link TabPersistentStore} to use.
     */
    void initForTesting(
            TabModelSelectorBase tabModelSelector,
            TabPersistentStore tabPersistentStore,
            TabPersistencePolicy tabPersistencePolicy,
            @Nullable TabPersistentStore shadowPersistentStore) {
        assert tabModelSelector != null;
        assert tabPersistentStore != null;
        assert tabPersistencePolicy != null;
        mTabModelSelector = tabModelSelector;
        mTabPersistentStore = tabPersistentStore;
        mTabPersistencePolicy = tabPersistencePolicy;
        mShadowTabPersistentStore = shadowPersistentStore;
    }

    private void recordTimeHistogram(String histogramName) {
        RecordHistogram.recordTimesHistogram(histogramName, uptimeMillis() - mInitializationTime);
    }
}
