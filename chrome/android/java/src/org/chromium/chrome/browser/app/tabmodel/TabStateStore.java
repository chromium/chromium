// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.task.PostTask.postTask;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab.TabStateStorageServiceFactory.createBatch;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.CombinedTabRestorer.CombinedTabRestorerDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.CollectionStorageObserverFactory;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageCollectionSynchronizer;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageRestoreOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabStripCollection;

import java.util.HashMap;
import java.util.Map;

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
    private final Map<Token, CollectionSaveForwarder> mGroupForwarderMap = new HashMap<>();
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void onIncognitoModelCreated() {
                    if (mInitRestoreOrchestratorForIncognito != null) {
                        mInitRestoreOrchestratorForIncognito.run();
                        mInitRestoreOrchestratorForIncognito = null;
                    }

                    boolean isAuthoritative =
                            ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource
                                    .getValue();
                    if (isAuthoritative || !mLoadIncognitoTabsOnStart) {
                        initVisualDataTracking(/* incognito= */ true);
                    }
                    if (!mLoadIncognitoTabsOnStart) {
                        initCollectionTracking(/* incognito= */ true);
                    }
                }

                @Override
                public void didBecomeEmpty() {
                    if (mIncognitoSynchronizer != null) {
                        mIncognitoSynchronizer.destroy();
                        mIncognitoSynchronizer = null;
                    }
                    mLoadIncognitoTabsOnStart = false;
                }
            };

    private @Nullable TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private @Nullable CombinedTabRestorer mCombinedTabRestorer;
    private @Nullable CombinedTabRestorer mMergeCombinedTabRestorer;
    private @Nullable Runnable mInitRestoreOrchestratorForIncognito;
    private @Nullable StorageCollectionSynchronizer mIncognitoSynchronizer;
    private @Nullable StorageCollectionSynchronizer mRegularSynchronizer;
    private int mRestoredTabCount;
    private boolean mIsDestroyed;
    private boolean mLoadIncognitoTabsOnStart;

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

    private final TabGroupModelFilterObserver mVisualDataUpdateObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    Token groupId = destinationTab.getTabGroupId();
                    assert groupId != null;

                    TabStripCollection collection = filter.getTabModel().getTabStripCollection();
                    if (collection == null) return;

                    CollectionSaveForwarder forwarder =
                            CollectionSaveForwarder.createForTabGroup(
                                    destinationTab.getProfile(), groupId, collection);
                    mGroupForwarderMap.put(groupId, forwarder);
                }

                @Override
                public void didRemoveTabGroup(
                        int oldRootId, @Nullable Token oldTabGroupId, int removalReason) {
                    if (oldTabGroupId == null) return;
                    CollectionSaveForwarder forwarder = mGroupForwarderMap.remove(oldTabGroupId);
                    if (forwarder != null) forwarder.destroy();
                }

                @Override
                public void didChangeTabGroupCollapsed(
                        Token tabGroupId, boolean isCollapsed, boolean animate) {
                    saveTabGroupPayload(tabGroupId);
                }

                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    saveTabGroupPayload(tabGroupId);
                }

                @Override
                public void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {
                    saveTabGroupPayload(tabGroupId);
                }
            };

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

        tabModelSelector.getModel(false).addObserver(mTabModelObserver);
        TabModel incognitoModel = tabModelSelector.getModel(true);
        incognitoModel.addObserver(mTabModelObserver);
        // Some tests use a MockTabModel.
        if (incognitoModel instanceof IncognitoTabModel itm) {
            itm.addIncognitoObserver(mIncognitoTabModelObserver);
        }
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
        mLoadIncognitoTabsOnStart = !ignoreIncognitoFiles;

        assert mCombinedTabRestorer == null;
        mCombinedTabRestorer =
                new CombinedTabRestorer(
                        mLoadIncognitoTabsOnStart,
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
        // Some tests use a MockTabModel.
        if (incognitoTabModel instanceof IncognitoTabModel itm) {
            itm.removeIncognitoObserver(mIncognitoTabModelObserver);
        }

        for (CollectionSaveForwarder forwarder : mGroupForwarderMap.values()) {
            forwarder.destroy();
        }

        for (boolean incognito : new boolean[] {false, true}) {
            TabGroupModelFilter filter = getFilter(incognito);
            if (filter != null) {
                filter.removeTabGroupObserver(mVisualDataUpdateObserver);
            }
        }

        if (mIncognitoSynchronizer != null) {
            mIncognitoSynchronizer.destroy();
        }
        if (mRegularSynchronizer != null) {
            mRegularSynchronizer.destroy();
        }
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

        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            TabGroupVisualDataStore.cacheGroups(data.getGroupsData());
            if (!incognito) {
                initRestoreOrchestrator(data, /* incognito= */ false);
                initVisualDataTracking(/* incognito= */ false);
            } else if (data.getLoadedTabStates().length != 0) {
                assert mLoadIncognitoTabsOnStart;

                // The incognito TabStripCollection is not created until the first incognito tab is
                // restored. If there are tabs to restore, the restore orchestrator needs to be
                // initialized when the TabStripCollection is created.
                mInitRestoreOrchestratorForIncognito =
                        () -> initRestoreOrchestrator(data, /* incognito= */ true);
            }
        }

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

        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            // The incognito model does not have a profile until after the first incognito tab is
            // created. If no incognito tabs were restored then tracking needs to be deferred until
            // the incognito model is created, this is taken care of the mIncognitoTabModelObserver
            // if it does not occur here.
            initCollectionTracking(/* incognito= */ false);

            if (mLoadIncognitoTabsOnStart) {
                initCollectionTracking(/* incognito= */ true);
            }
        } else {
            assert mTabRegistrationObserver == null;
            mTabRegistrationObserver =
                    new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
            mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                    new InnerRegistrationObserver());

            postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        fullSaveAndInitTracking(/* incognito= */ false);
                        if (!mLoadIncognitoTabsOnStart) return;
                        postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> fullSaveAndInitTracking(/* incognito= */ true));
                    });
        }
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

    private void saveTabGroupPayload(Token tabGroupId) {
        CollectionSaveForwarder forwarder = mGroupForwarderMap.get(tabGroupId);
        if (forwarder == null) return;
        forwarder.savePayload();
    }

    private void initVisualDataTracking(boolean incognito) {
        var profileAndCollection = getProfileAndCollection(incognito);
        TabGroupModelFilter filter = getFilter(incognito);
        assert filter != null;

        // Add forwarders for untracked groups.
        for (Token groupId : filter.getAllTabGroupIds()) {
            CollectionSaveForwarder forwarder =
                    CollectionSaveForwarder.createForTabGroup(
                            profileAndCollection.profile, groupId, profileAndCollection.collection);
            mGroupForwarderMap.put(groupId, forwarder);
        }

        filter.addTabGroupObserver(mVisualDataUpdateObserver);
    }

    private StorageCollectionSynchronizer getSynchronizer(
            ProfileAndCollection profileAndCollection, boolean incognito) {
        if (incognito) {
            if (mIncognitoSynchronizer != null) {
                return mIncognitoSynchronizer;
            }
            mIncognitoSynchronizer =
                    new StorageCollectionSynchronizer(
                            profileAndCollection.profile, profileAndCollection.collection);
            return mIncognitoSynchronizer;
        }

        if (mRegularSynchronizer != null) {
            return mRegularSynchronizer;
        }
        mRegularSynchronizer =
                new StorageCollectionSynchronizer(
                        profileAndCollection.profile, profileAndCollection.collection);
        return mRegularSynchronizer;
    }

    private void initRestoreOrchestrator(StorageLoadedData data, boolean incognito) {
        var profileAndCollection = getProfileAndCollection(incognito);
        var synchronizer = getSynchronizer(profileAndCollection, incognito);

        StorageRestoreOrchestratorFactory factory =
                new StorageRestoreOrchestratorFactory(
                        profileAndCollection.profile, profileAndCollection.collection, data);
        synchronizer.consumeRestoreOrchestratorFactory(factory);
    }

    private void initCollectionTracking(boolean incognito) {
        Profile profile = mTabModelSelector.getModel(incognito).getProfile();
        if (profile == null) return;

        var profileAndCollection = getProfileAndCollection(incognito);
        var synchronizer = getSynchronizer(profileAndCollection, incognito);

        CollectionStorageObserverFactory factory =
                new CollectionStorageObserverFactory(profileAndCollection.profile);
        synchronizer.consumeCollectionObserverFactory(factory);
    }

    private static class ProfileAndCollection {
        public final Profile profile;
        public final TabStripCollection collection;

        public ProfileAndCollection(Profile profile, TabStripCollection collection) {
            this.profile = profile;
            this.collection = collection;
        }
    }

    private ProfileAndCollection getProfileAndCollection(boolean incognito) {
        TabModel tabModel = mTabModelSelector.getModel(incognito);
        Profile profile = tabModel.getProfile();
        assert profile != null;
        TabStripCollection tabStripCollection = tabModel.getTabStripCollection();
        assert tabStripCollection != null;
        return new ProfileAndCollection(profile, tabStripCollection);
    }

    private @Nullable TabGroupModelFilter getFilter(boolean incognito) {
        return mTabModelSelector.getTabGroupModelFilterProvider().getTabGroupModelFilter(incognito);
    }

    private void fullSaveAndInitTracking(boolean incognito) {
        assert !ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue();

        Profile profile = mTabModelSelector.getModel(incognito).getProfile();
        if (profile == null) return;

        try (ScopedStorageBatch ignored = createBatch(profile)) {
            var profileAndCollection = getProfileAndCollection(incognito);
            getSynchronizer(profileAndCollection, incognito).fullSave();
        }
        initCollectionTracking(incognito);
        initVisualDataTracking(incognito);
    }
}
