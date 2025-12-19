// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.CombinedTabRestorer.CombinedTabRestorerDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** Orchestrates saving of tabs to the {@link TabStateStorageService}. */
@NullMarked
public class TabStateStore implements TabPersistentStore {
    private static final String TAG = "TabStateStore";

    private final TabStateStorageService mTabStateStorageService;
    private final TabCreatorManager mTabCreatorManager;
    private final TabModelSelector mTabModelSelector;
    private final String mWindowTag;
    private final TabPersistencePolicy mTabPersistencePolicy;
    private final TabStateAttributes.Observer mAttributesObserver =
            this::onTabStateDirtinessChanged;
    private final ObserverList<TabPersistentStoreObserver> mObservers = new ObserverList<>();

    private @Nullable TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private @Nullable CombinedTabRestorer mCombinedTabRestorer;
    private @Nullable CombinedTabRestorer mMergeCombinedTabRestorer;
    private int mRestoredTabCount;
    private boolean mIsDestroyed;

    private final ModelTrackingOrchestrator mModelTrackingManager;

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

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void willCloseAllTabs(boolean incognito) {
                    cancelLoadingTabs(incognito);
                }
            };

    private final CombinedTabRestorerDelegate mCombinedTabRestorerDelegate =
            new CombinedTabRestorerDelegate() {
                @Override
                public void onLoadFinished(int loadedTabCount) {
                    onAllDataLoaded(loadedTabCount);
                }

                @Override
                public void onCancelled() {
                    deleteDbIfNonAuthoritative();
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
     * @param tabStateStorageService The {@link TabStateStorageService} to save to.
     * @param tabModelSelector The {@link TabModelSelector} to observe changes in. Regardless of the
     *     mode this store is in, this will be the real selector with real models. This should be
     *     treated as a read only object, no modifications should go through it.
     * @param windowTag The window tag to use for the window.
     * @param tabCreatorManager Used to create new tabs on initial load. This may return real
     *     creators, or faked out creators if in non-authoritative mode.
     */
    public TabStateStore(
            TabStateStorageService tabStateStorageService,
            TabModelSelector tabModelSelector,
            String windowTag,
            TabCreatorManager tabCreatorManager,
            TabPersistencePolicy tabPersistencePolicy) {
        mTabStateStorageService = tabStateStorageService;
        mTabModelSelector = tabModelSelector;
        mWindowTag = windowTag;
        mTabCreatorManager = tabCreatorManager;
        mTabPersistencePolicy = tabPersistencePolicy;
        mModelTrackingManager = new ModelTrackingOrchestrator(tabModelSelector);

        tabModelSelector.getModel(false).addObserver(mTabModelObserver);
        TabModel incognitoModel = tabModelSelector.getModel(true);
        incognitoModel.addObserver(mTabModelObserver);
    }

    @Override
    public void onNativeLibraryReady() {
        // Native is already initialized in the constructor as the TabStateStorageService requires
        // native. This method is never called.
        assert false;
    }

    @Override
    public void waitForMigrationToFinish() {
        // Not relevant for this impl. This is used by other implementations that wait for updates
        // to the filesystem before proceeding. With this implementation the TabStateStorageService
        // is always available immediately.
    }

    @Override
    public void saveState() {
        // All mutations to the collection tree should already be queue to the DB thread so no
        // additional work is required for that.

        saveTabIfNotClean(mTabModelSelector.getModel(false).getCurrentTabSupplier().get());
        saveTabIfNotClean(mTabModelSelector.getModel(true).getCurrentTabSupplier().get());

        // If Chrome fully controlled its own lifecycle on Android we would block shutdown until the
        // DB task runner is flushed. The DB thread already has the BLOCK_SHUTDOWN trait, that does
        // not guarantee anything on Android; see https://crbug.com/40256943. A blocking wait
        // won't improve the background thread's chances of finishing and it would block other
        // shutdown work. The best we can do is manually boost the DB thread priority.
        mTabStateStorageService.boostPriority();
    }

    @Override
    public void loadState(boolean ignoreIncognitoFiles) {
        mModelTrackingManager.setLoadIncognitoTabsOnStart(!ignoreIncognitoFiles);

        assert mCombinedTabRestorer == null;
        mCombinedTabRestorer =
                new CombinedTabRestorer(
                        !ignoreIncognitoFiles,
                        mCombinedTabRestorerDelegate,
                        mTabCreatorManager,
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
    }

    @Override
    public void mergeState() {
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
        mMergeCombinedTabRestorer =
                new CombinedTabRestorer(
                        /* restoreIncognitoTabs= */ true,
                        delegate,
                        mTabCreatorManager,
                        /* logRestoreDuration= */ false);

        for (boolean incognito : new boolean[] {false, true}) {
            final boolean incognitoFinal = incognito;
            mTabStateStorageService.loadAllData(
                    windowTagToMerge,
                    incognitoFinal,
                    data -> {
                        if (mIsDestroyed) {
                            data.destroy();
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
        assert mCombinedTabRestorer != null;
        mCombinedTabRestorer.start(mTabModelSelector.isIncognitoSelected(), setActiveTab);
    }

    @Override
    public void restoreTabStateForUrl(String url) {
        if (mCombinedTabRestorer == null) return;
        mCombinedTabRestorer.restoreTabStateForUrl(url);
    }

    @Override
    public void restoreTabStateForId(int id) {
        if (mCombinedTabRestorer == null) return;
        mCombinedTabRestorer.restoreTabStateForId(id);
    }

    @Override
    public int getRestoredTabCount() {
        return mRestoredTabCount;
    }

    @Override
    public void clearState() {
        // Clearing the state globally is intentional.
        mTabStateStorageService.clearState();
    }

    private void cancelLoadingTabs(boolean incognito) {
        if (mCombinedTabRestorer == null) return;

        mCombinedTabRestorer.cancelLoadingTabs(incognito);
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
        TabModel incognitoTabModel = mTabModelSelector.getModel(true);
        incognitoTabModel.removeObserver(mTabModelObserver);

        mModelTrackingManager.destroy();
    }

    @Override
    public void pauseSaveTabList() {
        // TODO(https://crbug.com/448151052): This should freeze saves for the collection tree until
        // resumed. If we have proper batching it might not be necessary to pause.
    }

    @Override
    public void resumeSaveTabList(Runnable onSaveTabListRunnable) {
        // TODO(https://crbug.com/448151052): This should catch up on saves for the collection tree
        // after a pause. If we have proper batching it might not be necessary to pause.
    }

    @Override
    public void cleanupStateFile(int windowId) {
        // The archived tab state file does not support this operation.
        assert windowId != TabWindowManager.INVALID_WINDOW_ID;
        String windowTag = Integer.toString(windowId);
        mTabStateStorageService.clearWindow(windowTag);
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
        // If a tab is not in a closing or destroyed state we shouldn't save it. Tabs that are
        // not attached to a parent collection will not be restored at startup and shouldn't be
        // saved. If the tab becomes attached to a collection later it will be saved then.
        if (tab.isDestroyed() || tab.isClosing() || !tab.hasParentCollection()) return;
        mTabStateStorageService.saveTabData(tab);
    }

    private void onTabRegistered(Tab tab) {
        TabStateAttributes attributes = TabStateAttributes.from(tab);
        assumeNonNull(attributes);
        // Save every clean tab on registration if we are not authoritative, we are catching up.
        if (attributes.addObserver(mAttributesObserver) == DirtinessState.DIRTY
                || !ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource
                        .getValue()) {
            saveTab(tab);
        }
    }

    private void onTabUnregistered(Tab tab) {
        if (!tab.isDestroyed()) {
            assumeNonNull(TabStateAttributes.from(tab)).removeObserver(mAttributesObserver);
        }
        // TODO(https://crbug.com/430996004): If closing, delete the tab record.
    }

    /** Called when the data for one of the models has been loaded. */
    private void onDataLoaded(StorageLoadedData data, boolean incognito) {
        if (mIsDestroyed) {
            data.destroy();
            return;
        }

        mModelTrackingManager.onDataLoaded(data, incognito);

        assumeNonNull(mCombinedTabRestorer);
        mCombinedTabRestorer.onDataLoaded(data, incognito);
    }

    /** Called after both the regular and incognito data has been loaded. */
    private void onAllDataLoaded(int loadedTabCount) {
        mRestoredTabCount = loadedTabCount;
        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onInitialized(mRestoredTabCount);
        }

        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
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

        if (!ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            assert mTabRegistrationObserver == null;
            mTabRegistrationObserver =
                    new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
            mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                    new InnerRegistrationObserver());
        }

        mModelTrackingManager.onRestoreFinished();
    }

    private void deleteDbIfNonAuthoritative() {
        if (!ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            // When we aren't the authoritative source we don't trust ourselves to be correct.
            // Raze the db and rebuild from the loaded tab state to ensure we are in a known good
            // state. This is a no-op if we are the authoritative source as there shouldn't be a
            // delta and if there is we need a less blunt mechanism to reconcile the difference.
            clearState();
        }
    }
}
