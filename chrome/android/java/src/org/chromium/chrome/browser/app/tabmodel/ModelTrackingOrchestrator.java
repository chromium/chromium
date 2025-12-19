// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.task.PostTask.postTask;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab.TabStateStorageServiceFactory.createBatch;

import androidx.annotation.IntDef;

import org.chromium.base.Token;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.CollectionStorageObserverFactory;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageCollectionSynchronizer;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageRestoreOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabStripCollection;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/**
 * Manages the tracking lifecycles of {@link StorageCollectionSynchronizer}s and orchestrates the
 * visual data update tracking for both the incognito and regular {@link TabModel}s to storage.
 */
@NullMarked
public class ModelTrackingOrchestrator {
    /** The state of the synchronization lifecycle for a specific model. */
    @IntDef({
        SynchronizerState.START,
        SynchronizerState.MODEL_PENDING,
        SynchronizerState.RESTORING,
        SynchronizerState.TRACKING
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface SynchronizerState {
        /**
         * Initial state before any data has been loaded or after a tab model has been destroyed.
         */
        int START = 0;

        /**
         * Data has been loaded from storage, but the TabModel has not been created yet. Only
         * attainable for the incognito model.
         */
        int MODEL_PENDING = 1;

        /**
         * The TabModel has been created and tabs are currently being restored from storage. Changes
         * to the TabModel are tracked during this time.
         */
        int RESTORING = 2;

        /** Tracking changes to the TabModel. */
        int TRACKING = 3;
    }

    /** Used to manage the orchestration lifecycle of a Synchronizer. */
    private interface SynchronizerManager {
        /**
         * Called when the TabStateStorageService has loaded the data for this model.
         *
         * @param data The loaded tab and group data.
         */
        void onDataLoaded(StorageLoadedData data);

        /**
         * Called when the TabModel associated with this orchestrator has been created. For Regular
         * models, this is immediate. For Incognito, can be deferred until the first incognito tab
         * is instantiated.
         */
        default void onModelCreated() {}

        /** Called when the CombinedTabRestorer has finished restoring all tabs for this model. */
        void onRestoreFinished();

        /** Destroys the synchronizer and resets the manager state. */
        void reset();
    }

    private final TabModelSelector mTabModelSelector;
    private final Map<Token, CollectionSaveForwarder> mGroupForwarderMap = new HashMap<>();
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void onIncognitoModelCreated() {
                    mIncognitoSynchronizerManager.onModelCreated();
                }

                @Override
                public void didBecomeEmpty() {
                    mIncognitoSynchronizerManager.reset();
                }
            };

    private @Nullable StorageCollectionSynchronizer mIncognitoSynchronizer;
    private @Nullable StorageCollectionSynchronizer mRegularSynchronizer;
    private boolean mLoadIncognitoTabsOnStart;

    private final SynchronizerManager mRegularSynchronizerManager =
            new RegularSynchronizerManager();
    private final SynchronizerManager mIncognitoSynchronizerManager =
            new IncognitoSynchronizerManager();

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

    /**
     * @param tabModelSelector The {@link TabModelSelector} to observe changes for.
     */
    public ModelTrackingOrchestrator(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;

        TabModel incognitoModel = tabModelSelector.getModel(true);
        if (incognitoModel instanceof IncognitoTabModel itm) {
            itm.addIncognitoObserver(mIncognitoTabModelObserver);
        }
    }

    /**
     * Sets whether to load incognito tabs on start.
     *
     * @param loadIncognitoTabsOnStart Whether the incognito tab model has tabs to load on start.
     */
    public void setLoadIncognitoTabsOnStart(boolean loadIncognitoTabsOnStart) {
        mLoadIncognitoTabsOnStart = loadIncognitoTabsOnStart;
    }

    /**
     * Called when the {@link TabStateStorageService} has loaded the data for a model.
     *
     * @param data The loaded tab and group data.
     * @param incognito Whether the data is for an incognito model.
     */
    public void onDataLoaded(StorageLoadedData data, boolean incognito) {
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            TabGroupVisualDataStore.cacheGroups(data.getGroupsData());
        }

        if (incognito) {
            mIncognitoSynchronizerManager.onDataLoaded(data);
        } else {
            mRegularSynchronizerManager.onDataLoaded(data);
        }
    }

    /**
     * Called when the {@link CombinedTabRestorer} has finished restoring all tabs for both models.
     */
    public void onRestoreFinished() {
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            mRegularSynchronizerManager.onRestoreFinished();
            mIncognitoSynchronizerManager.onRestoreFinished();
        } else {
            postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        mRegularSynchronizerManager.onRestoreFinished();
                        postTask(
                                TaskTraits.UI_DEFAULT,
                                mIncognitoSynchronizerManager::onRestoreFinished);
                    });
        }
    }

    /** Performs the cleanup required for the synchronizers when the TabStateStore is destroyed. */
    public void destroy() {
        TabModel incognitoModel = mTabModelSelector.getModel(true);
        if (incognitoModel instanceof IncognitoTabModel itm) {
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

        mRegularSynchronizerManager.reset();
        mIncognitoSynchronizerManager.reset();
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
        var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
        var synchronizer = getSynchronizer(profileAndCollection, incognito);

        StorageRestoreOrchestratorFactory factory =
                new StorageRestoreOrchestratorFactory(
                        profileAndCollection.profile, profileAndCollection.collection, data);
        synchronizer.consumeRestoreOrchestratorFactory(factory);
    }

    private void initCollectionTracking(boolean incognito) {
        Profile profile = mTabModelSelector.getModel(incognito).getProfile();
        if (profile == null) return;

        var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
        var synchronizer = getSynchronizer(profileAndCollection, incognito);

        CollectionStorageObserverFactory factory =
                new CollectionStorageObserverFactory(profileAndCollection.profile);
        synchronizer.consumeCollectionObserverFactory(factory);
    }

    private void fullSaveAndInitTracking(boolean incognito) {
        assert !ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue();

        Profile profile = mTabModelSelector.getModel(incognito).getProfile();
        if (profile == null) return;

        try (ScopedStorageBatch ignored = createBatch(profile)) {
            var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
            getSynchronizer(profileAndCollection, incognito).fullSave();
        }
        initCollectionTracking(incognito);
        initVisualDataTracking(incognito);
    }

    private void saveTabGroupPayload(Token tabGroupId) {
        CollectionSaveForwarder forwarder = mGroupForwarderMap.get(tabGroupId);
        if (forwarder == null) return;
        forwarder.savePayload();
    }

    private void initVisualDataTracking(boolean incognito) {
        var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
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

    private @Nullable TabGroupModelFilter getFilter(boolean incognito) {
        return mTabModelSelector.getTabGroupModelFilterProvider().getTabGroupModelFilter(incognito);
    }

    private static ProfileAndCollection getProfileAndCollection(
            TabModelSelector selector, boolean incognito) {
        TabModel tabModel = selector.getModel(incognito);
        Profile profile = tabModel.getProfile();
        assert profile != null;
        TabStripCollection tabStripCollection = tabModel.getTabStripCollection();
        assert tabStripCollection != null;
        return new ProfileAndCollection(profile, tabStripCollection);
    }

    private static class ProfileAndCollection {
        public final Profile profile;
        public final TabStripCollection collection;

        public ProfileAndCollection(Profile profile, TabStripCollection collection) {
            this.profile = profile;
            this.collection = collection;
        }
    }

    /** Manages the Synchronizer for the Regular {@link TabModel}. */
    private class RegularSynchronizerManager implements SynchronizerManager {
        private @SynchronizerState int mState = SynchronizerState.START;

        @Override
        public void onDataLoaded(StorageLoadedData data) {
            if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
                assert mState == SynchronizerState.START;
                mState = SynchronizerState.RESTORING;
                initRestoreOrchestrator(data, /* incognito= */ false);
                initVisualDataTracking(/* incognito= */ false);
            }
        }

        @Override
        public void onRestoreFinished() {
            if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
                assert mState == SynchronizerState.RESTORING;
                mState = SynchronizerState.TRACKING;
                initCollectionTracking(/* incognito= */ false);
            } else {
                assert mState == SynchronizerState.START;
                fullSaveAndInitTracking(/* incognito= */ false);
                mState = SynchronizerState.TRACKING;
            }
        }

        @Override
        public void reset() {
            if (mRegularSynchronizer != null) {
                mRegularSynchronizer.destroy();
                mRegularSynchronizer = null;
            }
            mState = SynchronizerState.START;
        }
    }

    /** Manages the Synchronizer for the Incognito {@link TabModel}. */
    private class IncognitoSynchronizerManager implements SynchronizerManager {
        private @SynchronizerState int mState = SynchronizerState.START;
        private @Nullable Runnable mInitRestoreOrchestratorCallback;

        @Override
        public void onDataLoaded(StorageLoadedData data) {
            if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
                if (data.getLoadedTabStates().length > 0) {
                    assert mState == SynchronizerState.START;
                    mState = SynchronizerState.MODEL_PENDING;
                    mInitRestoreOrchestratorCallback =
                            () -> initRestoreOrchestrator(data, /* incognito= */ true);
                }
            }
        }

        @Override
        public void onModelCreated() {
            boolean authoritative =
                    ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue();
            if (authoritative) {
                if (mState == SynchronizerState.MODEL_PENDING) {
                    mState = SynchronizerState.RESTORING;
                    assumeNonNull(mInitRestoreOrchestratorCallback).run();
                    initVisualDataTracking(/* incognito= */ true);
                    mInitRestoreOrchestratorCallback = null;
                } else if (mState == SynchronizerState.START) {
                    mState = SynchronizerState.TRACKING;
                    initVisualDataTracking(/* incognito= */ true);
                    initCollectionTracking(/* incognito= */ true);
                }
            } else {
                if (mState == SynchronizerState.START && !mLoadIncognitoTabsOnStart) {
                    mState = SynchronizerState.TRACKING;
                    initVisualDataTracking(/* incognito= */ true);
                    initCollectionTracking(/* incognito= */ true);
                }
            }
        }

        @Override
        public void onRestoreFinished() {
            if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
                if (mState == SynchronizerState.RESTORING) {
                    mState = SynchronizerState.TRACKING;
                    initCollectionTracking(/* incognito= */ true);
                }
            } else {
                if (mState == SynchronizerState.START && mLoadIncognitoTabsOnStart) {
                    fullSaveAndInitTracking(/* incognito= */ true);
                    mState = SynchronizerState.TRACKING;
                }
            }
            mLoadIncognitoTabsOnStart = false;
        }

        @Override
        public void reset() {
            if (mIncognitoSynchronizer != null) {
                mIncognitoSynchronizer.destroy();
                mIncognitoSynchronizer = null;
            }

            mState = SynchronizerState.START;
            mLoadIncognitoTabsOnStart = false;
            mInitRestoreOrchestratorCallback = null;
        }
    }
}
