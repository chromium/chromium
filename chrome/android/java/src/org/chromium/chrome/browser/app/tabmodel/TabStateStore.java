// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.CombinedTabRestorer.CombinedTabRestorerDelegate;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadingStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.List;
import java.util.Locale;

/** Orchestrates saving of tabs to the {@link TabStateStorageService}. */
@NullMarked
public class TabStateStore implements TabPersistentStore {
    private static final String TAG = "TabStateStore";
    private static final String RESTORED_TAB_COUNT_DELTA =
            "Tabs.TabStateStore.InternalTabCountDelta.";

    private @MonotonicNonNull TabStateStorageService mTabStateStorageService;
    private final PersistentStoreMigrationManager mMigrationManager;
    private final TabCreatorManager mTabCreatorManager;
    private final TabModelSelector mTabModelSelector;
    private final String mWindowTag;
    private final TabCountTracker mTabCountTracker;
    private final ModelTrackingOrchestrator.Factory mOrchestratorFactory;
    private final TabPersistencePolicy mTabPersistencePolicy;
    private final @Nullable CipherFactory mCipherFactory;
    private final boolean mIsAuthoritative;
    private final TabStateAttributes.Observer mAttributesObserver =
            this::onTabStateDirtinessChanged;
    private final ObserverList<TabPersistentStoreObserver> mObservers = new ObserverList<>();
    private @MonotonicNonNull ModelTrackingOrchestrator mModelTrackingManager;
    private boolean mHasCipherFactory;

    private @Nullable TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private @Nullable CombinedTabRestorer mCombinedTabRestorer;
    private @Nullable CombinedTabRestorer mMergeCombinedTabRestorer;
    private int mRestoredTabCount;
    private boolean mIsDestroyed;

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void onTabCloseUndone(List<Tab> tabs, boolean isAllTabs) {
                    if (tabs.isEmpty()) return;
                    updateTabCountForModel(tabs.get(0).isOffTheRecord());
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    updateTabCountForModel(tab.isOffTheRecord());
                }

                @Override
                public void willCloseAllTabs(boolean incognito) {
                    cancelLoadingTabs(incognito);
                }
            };

    private class InnerRegistrationObserver
            implements TabModelSelectorTabRegistrationObserver.Observer {
        @Override
        public void onTabRegistered(Tab tab) {
            TabStateStore.this.onTabRegistered(tab);
        }

        @Override
        public void onTabUnregistered(Tab tab) {
            TabStateStore.this.onTabUnregistered(tab);
        }
    }

    private final CombinedTabRestorerDelegate mCombinedTabRestorerDelegate =
            new CombinedTabRestorerDelegate() {
                @Override
                public void onLoadFinished(int loadedTabCount) {
                    onAllDataLoaded(loadedTabCount);
                }

                @Override
                public void onCancelled() {
                    assumeNonNull(mModelTrackingManager).onRestoreCancelled();
                    deleteDbIfNonAuthoritative();
                }

                @Override
                public void onRestoredForModel(boolean incognito) {
                    if (!mIsAuthoritative) return;
                    assumeNonNull(mModelTrackingManager).onRestoredForModel(incognito);
                }

                @Override
                public void onActiveTabRestored(boolean incognito) {
                    for (TabPersistentStoreObserver observer : mObservers) {
                        observer.onActiveTabLoaded(incognito);
                    }
                }

                @Override
                public void onRestoreFinished() {
                    onFinishedCreatingAllTabs();
                }

                @Override
                public void onDetailsRead(
                        int index,
                        @TabId int tabId,
                        String url,
                        boolean isStandardActiveIndex,
                        boolean isIncognitoActiveIndex,
                        boolean isIncognito,
                        boolean fromMerge) {
                    for (TabPersistentStoreObserver observer : mObservers) {
                        observer.onDetailsRead(
                                index,
                                tabId,
                                url,
                                isStandardActiveIndex,
                                isIncognitoActiveIndex,
                                isIncognito,
                                fromMerge);
                    }
                }
            };

    /**
     * @param tabModelSelector The {@link TabModelSelector} to observe changes in. Regardless of the
     *     mode this store is in, this will be the real selector with real models. This should be
     *     treated as a read only object, no modifications should go through it.
     * @param windowTag The window tag to use for the window.
     * @param tabCreatorManager Used to create new tabs on initial load. This may return real
     *     creators, or faked out creators if in non-authoritative mode.
     * @param tabPersistencePolicy The {@link TabPersistencePolicy} to use for the window.
     * @param migrationManager The migration manager for the window.
     * @param cipherFactory The {@link CipherFactory} to use for encryption. If null, it will not be
     *     possible to load/save off the record nodes.
     * @param isAuthoritative Whether this store is the authoritative store for the window.
     * @param orchestratorFactory The factory to create {@link ModelTrackingOrchestrator} instances.
     * @param isAuthoritative Whether the store is authoritative for the window.
     */
    public TabStateStore(
            TabModelSelector tabModelSelector,
            String windowTag,
            TabCreatorManager tabCreatorManager,
            TabPersistencePolicy tabPersistencePolicy,
            PersistentStoreMigrationManager migrationManager,
            @Nullable CipherFactory cipherFactory,
            TabCountTracker tabCountTracker,
            ModelTrackingOrchestrator.Factory orchestratorFactory,
            boolean isAuthoritative) {
        mTabModelSelector = tabModelSelector;
        mWindowTag = windowTag;
        mTabCreatorManager = tabCreatorManager;
        mTabPersistencePolicy = tabPersistencePolicy;
        mMigrationManager = migrationManager;
        mCipherFactory = cipherFactory;
        mIsAuthoritative = isAuthoritative;
        mOrchestratorFactory = orchestratorFactory;
        mTabCountTracker = tabCountTracker;
    }

    @Initializer
    @Override
    public void onNativeLibraryReady() {
        // Prevent calling this method again after initialization.
        if (mTabStateStorageService != null) return;

        Profile profile = mTabModelSelector.getModel(/* incognito= */ false).getProfile();
        assert profile != null;
        TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(profile);
        assert service != null;
        mTabStateStorageService = service;

        if (mCipherFactory != null) {
            byte[] key = mCipherFactory.getKeyForTabStateStorage();
            if (key == null) {
                key = mTabStateStorageService.generateKey(mWindowTag);
                mCipherFactory.setKeyForTabStateStorage(key);
            } else {
                mTabStateStorageService.setKey(mWindowTag, key);
            }
            mHasCipherFactory = true;
        } else {
            mHasCipherFactory = false;
        }
        mModelTrackingManager =
                mOrchestratorFactory.build(
                        mWindowTag,
                        mMigrationManager,
                        mTabModelSelector,
                        mHasCipherFactory,
                        mIsAuthoritative);

        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
        mTabModelSelector.getModel(true).addObserver(mTabModelObserver);
    }

    @Override
    public void waitForMigrationToFinish() {
        // Not relevant for this impl. This is used by other implementations that wait for updates
        // to the filesystem before proceeding. With this implementation the TabStateStorageService
        // is always available immediately.
    }

    @Override
    public void saveState() {
        assertInitialized();

        // All mutations to the collection tree should already be queue to the DB thread so no
        // additional work is required for that.

        saveTabIfNotClean(mTabModelSelector.getModel(false).getCurrentTabSupplier().get());
        if (mModelTrackingManager.isSynchronizerPresent(/* incognito= */ true)) {
            saveTabIfNotClean(mTabModelSelector.getModel(true).getCurrentTabSupplier().get());
        }

        // If Chrome fully controlled its own lifecycle on Android we would block shutdown until the
        // DB task runner is flushed. The DB thread already has the BLOCK_SHUTDOWN trait, that does
        // not guarantee anything on Android; see https://crbug.com/40256943. A blocking wait
        // won't improve the background thread's chances of finishing and it would block other
        // shutdown work. The best we can do is manually boost the DB thread priority.
        mTabStateStorageService.boostPriority();
    }

    @Override
    public void loadState(boolean ignoreIncognitoFiles) {
        assertInitialized();

        ignoreIncognitoFiles |= !mHasCipherFactory;
        mModelTrackingManager.setLoadIncognitoTabsOnStart(!ignoreIncognitoFiles);

        mRestoredTabCount = mTabCountTracker.getRestoredTabCount(/* incognito= */ false);
        if (!ignoreIncognitoFiles) {
            mRestoredTabCount += mTabCountTracker.getRestoredTabCount(/* incognito= */ true);
        }

        assert mCombinedTabRestorer == null;
        mCombinedTabRestorer =
                new CombinedTabRestorer(
                        !ignoreIncognitoFiles,
                        mCombinedTabRestorerDelegate,
                        mTabCreatorManager,
                        mTabStateStorageService::createBatch,
                        /* logRestoreDuration= */ true);

        boolean[] restoreOrder =
                mTabModelSelector.isIncognitoSelected()
                        ? new boolean[] {true, false}
                        : new boolean[] {false, true};
        for (boolean incognito : restoreOrder) {
            if (incognito && ignoreIncognitoFiles) continue;
            mTabStateStorageService.loadAllData(
                    mWindowTag, incognito, data -> onDataLoaded(data, incognito));
        }

        if (ignoreIncognitoFiles) {
            mTabCountTracker.clearTabCount(/* incognito= */ true);
            mTabStateStorageService.clearUnusedNodesForWindow(
                    mWindowTag, /* isOffTheRecord= */ true, /* tabStripCollection= */ null);
        }
    }

    @Override
    public void mergeState() {
        assertInitialized();
        if (mCombinedTabRestorer != null) {
            Log.e(TAG, "mergeState aborted as initial restore is in progress.");
            return;
        } else if (mMergeCombinedTabRestorer != null || mTabPersistencePolicy.isMergeInProgress()) {
            Log.e(TAG, "mergeState aborted as merge restore is in progress.");
            return;
        }
        mTabPersistencePolicy.setMergeInProgress(true);

        String windowTagToMerge = mTabPersistencePolicy.getWindowTagToBeMerged();
        assert windowTagToMerge != null : "Window tag must be non-null if merge is enabled.";

        assert mMergeCombinedTabRestorer == null;
        CombinedTabRestorerDelegate delegate =
                new CombinedTabRestorerDelegate() {
                    @Override
                    public void onLoadFinished(int loadedTabCount) {
                        mTabStateStorageService.clearWindow(windowTagToMerge);
                    }

                    @Override
                    public void onRestoreFinished() {
                        mTabPersistencePolicy.setMergeInProgress(false);
                        mMergeCombinedTabRestorer = null;
                    }
                };

        // TODO(crbug.com/463956290): Confirm the key for incognito tabs is valid.
        assertOtrOperationSafe(/* isOtrOperation= */ true);
        mMergeCombinedTabRestorer =
                new CombinedTabRestorer(
                        /* restoreIncognitoTabs= */ true,
                        delegate,
                        mTabCreatorManager,
                        mTabStateStorageService::createBatch,
                        /* logRestoreDuration= */ false);

        for (boolean incognito : new boolean[] {false, true}) {
            final boolean incognitoFinal = incognito;
            mTabStateStorageService.loadAllData(
                    windowTagToMerge,
                    incognitoFinal,
                    data -> {
                        if (mIsDestroyed) {
                            fullyDestroyLoadedData(data);
                            return;
                        }
                        assumeNonNull(mMergeCombinedTabRestorer);
                        mMergeCombinedTabRestorer.onDataLoaded(data, incognitoFinal);
                    });
        }
        mMergeCombinedTabRestorer.start(
                mTabModelSelector.isIncognitoSelected(), /* restoreActiveTabImmediately= */ false);
    }

    @Override
    public void restoreTabs(boolean setActiveTab) {
        assertInitialized();
        assert mCombinedTabRestorer != null;
        mCombinedTabRestorer.start(mTabModelSelector.isIncognitoSelected(), setActiveTab);
    }

    @Override
    public void restoreTabStateForUrl(String url) {
        assertInitialized();
        if (mCombinedTabRestorer == null) return;
        mCombinedTabRestorer.restoreTabStateForUrl(url);
    }

    @Override
    public void restoreTabStateForId(int id) {
        assertInitialized();
        if (mCombinedTabRestorer == null) return;
        mCombinedTabRestorer.restoreTabStateForId(id);
    }

    @Override
    public int getRestoredTabCount() {
        assertInitialized();
        return mRestoredTabCount;
    }

    @Override
    public void clearState() {
        assertInitialized();

        // Clearing the state globally is intentional.
        mTabStateStorageService.clearState();
        TabCountTracker.clearGlobalState();
    }

    private void cancelLoadingTabs(boolean incognito) {
        assertInitialized();
        if (mCombinedTabRestorer != null) {
            mCombinedTabRestorer.cancelLoadingTabs(incognito);
        }
    }

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        mIsDestroyed = true;

        if (mTabRegistrationObserver != null) {
            mTabRegistrationObserver.destroy();
        }

        if (mCombinedTabRestorer != null) {
            mCombinedTabRestorer.cancel();
            mCombinedTabRestorer = null;
        }

        if (mMergeCombinedTabRestorer != null) {
            mMergeCombinedTabRestorer.cancel();
            mMergeCombinedTabRestorer = null;
            // Reset if an ongoing merge gets cancelled.
            mTabPersistencePolicy.setMergeInProgress(false);
        }

        mTabModelSelector.getModel(false).removeObserver(mTabModelObserver);
        mTabModelSelector.getModel(true).removeObserver(mTabModelObserver);

        if (mModelTrackingManager != null) {
            mModelTrackingManager.destroy();
        }
    }

    @Override
    public void pauseSaveTabList() {
        // TODO(https://crbug.com/448151052): This should freeze saves for the collection tree until
        // resumed. If we have proper batching it might not be necessary to pause.
        assertInitialized();
    }

    @Override
    public void resumeSaveTabList(Runnable onSaveTabListRunnable) {
        // TODO(https://crbug.com/448151052): This should catch up on saves for the collection tree
        // after a pause. If we have proper batching it might not be necessary to pause.
        assertInitialized();
    }

    @Override
    public void cleanupStateFile(int windowId) {
        assertInitialized();

        // The archived tab state file does not support this operation.
        assert windowId != TabWindowManager.INVALID_WINDOW_ID;
        String windowTag = Integer.toString(windowId);

        mTabStateStorageService.clearWindow(windowTag);
        TabCountTracker.cleanupWindow(windowTag);
    }

    @Override
    public void clearCurrentWindow() {
        assertInitialized();

        mTabStateStorageService.clearWindow(mWindowTag);
        mTabCountTracker.clearCurrentWindow();
    }

    @Override
    public void addObserver(TabPersistentStoreObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabPersistentStoreObserver observer) {
        mObservers.removeObserver(observer);
    }

    private void onTabStateDirtinessChanged(Tab tab, @DirtinessState int dirtiness) {
        if (dirtiness == DirtinessState.DIRTY && !tab.isDestroyed()) {
            saveTab(tab);
        }
    }

    private void saveTabIfNotClean(@Nullable Tab tab) {
        if (tab == null) return;

        TabStateAttributes attributes = TabStateAttributes.from(tab);
        assumeNonNull(attributes);
        if (attributes.getDirtinessState() != DirtinessState.CLEAN) {
            saveTab(tab);
        }
    }

    private void saveTab(Tab tab) {
        assertInitialized();
        assertOtrOperationSafe(tab.isOffTheRecord());
        // If a tab is not in a closing or destroyed state we shouldn't save it. Tabs that are
        // not attached to a parent collection will not be restored at startup and shouldn't be
        // saved. If the tab becomes attached to a collection later it will be saved then.
        if (tab.isDestroyed() || tab.isClosing() || !tab.hasParentCollection()) return;
        mModelTrackingManager.saveTab(tab);
    }

    private void assertOtrOperationSafe(boolean isOtrOperation) {
        assert !isOtrOperation || mHasCipherFactory;
    }

    private void onTabRegistered(Tab tab) {
        boolean isTabOtr = tab.isOffTheRecord();
        assertOtrOperationSafe(isTabOtr);

        TabStateAttributes attributes = TabStateAttributes.from(tab);
        assumeNonNull(attributes);
        // Save every clean tab on registration if we are not authoritative, we are catching up.
        if (attributes.addObserver(mAttributesObserver) == DirtinessState.DIRTY
                || mIsAuthoritative) {
            saveTab(tab);
        }
        updateTabCountForModel(isTabOtr);
    }

    private void onTabUnregistered(Tab tab) {
        if (!tab.isDestroyed()) {
            assumeNonNull(TabStateAttributes.from(tab)).removeObserver(mAttributesObserver);
        }
        // TODO(https://crbug.com/430996004): If closing, delete the tab record.
        updateTabCountForModel(tab.isOffTheRecord());
    }

    /** Called when the data for one of the models has been loaded. */
    private void onDataLoaded(StorageLoadedData data, boolean incognito) {
        assertInitialized();
        assertOtrOperationSafe(incognito);

        if (data.getLoadingStatus() != StorageLoadingStatus.SUCCESS) {
            mTabStateStorageService.clearUnusedNodesForWindow(
                    mWindowTag, incognito, /* tabStripCollection= */ null);
            mTabCountTracker.clearTabCount(incognito);
            String formattedErrorMessage =
                    String.format(
                            Locale.ROOT,
                            "Failed to load data with error code %d: %s",
                            data.getLoadingStatus(),
                            assumeNonNull(data.getErrorMessage()));
            Log.e(TAG, formattedErrorMessage);

            if (!mIsAuthoritative) {
                mMigrationManager.onShadowStoreRazed();
            }
            fullyDestroyLoadedData(data);

            // Leave to guarantee failures are caught in debug.
            assert false : formattedErrorMessage;
            return;
        }

        if (mIsDestroyed) {
            fullyDestroyLoadedData(data);
            return;
        }

        assumeNonNull(mModelTrackingManager).onDataLoaded(data, incognito);

        assumeNonNull(mCombinedTabRestorer);
        mCombinedTabRestorer.onDataLoaded(data, incognito);
    }

    /** Called after both the regular and incognito data has been loaded. */
    private void onAllDataLoaded(int loadedTabCount) {
        assertInitialized();

        if (mMigrationManager.isShadowStoreCaughtUp() || mIsAuthoritative) {
            int tabCountDelta = loadedTabCount - mRestoredTabCount;
            if (tabCountDelta > 0) {
                RecordHistogram.recordCount1000Histogram(
                        RESTORED_TAB_COUNT_DELTA + "DatabaseHigher", tabCountDelta);
            } else if (tabCountDelta < 0) {
                RecordHistogram.recordCount1000Histogram(
                        RESTORED_TAB_COUNT_DELTA + "CounterHigher", -tabCountDelta);
            }
        }

        mRestoredTabCount = loadedTabCount;
        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onInitialized(mRestoredTabCount);
        }

        if (mIsAuthoritative) {
            assert mTabRegistrationObserver == null;
            mTabRegistrationObserver =
                    new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
            mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                    new InnerRegistrationObserver());
        }
    }

    /** Called after all tabs have been created. */
    private void onFinishedCreatingAllTabs() {
        deleteDbIfNonAuthoritative();

        if (mIsDestroyed) return;

        mCombinedTabRestorer = null;

        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onStateLoaded();
        }

        if (!mIsAuthoritative) {
            assert mTabRegistrationObserver == null;
            mTabRegistrationObserver =
                    new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
            mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                    new InnerRegistrationObserver());
        }

        assumeNonNull(mModelTrackingManager).onRestoreFinished();
    }

    private void deleteDbIfNonAuthoritative() {
        assertInitialized();

        if (!mIsAuthoritative) {
            // When we aren't the authoritative source we don't trust ourselves to be correct.
            // Raze the db and rebuild from the loaded tab state to ensure we are in a known good
            // state. This is a no-op if we are the authoritative source as there shouldn't be a
            // delta and if there is we need a less blunt mechanism to reconcile the difference.
            clearCurrentWindow();
        }
    }

    @EnsuresNonNull({"mTabStateStorageService", "mModelTrackingManager"})
    private void assertInitialized() {
        assert mTabStateStorageService != null && mModelTrackingManager != null
                : "The store has not been initialized";
    }

    private void fullyDestroyLoadedData(StorageLoadedData data) {
        assumeNonNull(mModelTrackingManager).onRestoreCancelled();
        StorageLoadedData.LoadedTabState[] loadedTabStates = data.getLoadedTabStates();
        for (StorageLoadedData.LoadedTabState loadedTabState : loadedTabStates) {
            WebContentsState contentsState = loadedTabState.tabState.contentsState;
            if (contentsState == null) continue;
            contentsState.destroy();
        }
        data.destroy();
    }

    private void updateTabCountForModel(boolean incognito) {
        assertInitialized();

        if (!mModelTrackingManager.isSynchronizerPresent(incognito)) return;
        int tabCountForModel = mTabModelSelector.getModel(incognito).getCount();
        mTabCountTracker.updateTabCount(incognito, tabCountForModel);
    }
}
