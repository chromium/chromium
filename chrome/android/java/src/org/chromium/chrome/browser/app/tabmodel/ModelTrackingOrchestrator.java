// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tab.TabStateStorageServiceFactory.createBatch;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.task.ChainedTasks;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.CollectionStorageObserverFactory;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.StorageCollectionSynchronizer;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageRestoreOrchestratorFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
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
    /** Factory for creating a {@link ModelTrackingOrchestrator}. */
    @FunctionalInterface
    public interface Factory {
        /**
         * @param windowTag The window tag to use for the window.
         * @param migrationManager The migration manager for the window.
         * @param tabModelSelector The {@link TabModelSelector} to observe changes for.
         * @param hasCipherFactory Whether a cipher factory was provided for OTR data.
         * @param isAuthoritative Whether this store is the authoritative store for the window.
         */
        ModelTrackingOrchestrator build(
                String windowTag,
                PersistentStoreMigrationManager migrationManager,
                TabModelSelector tabModelSelector,
                boolean hasCipherFactory,
                boolean isAuthoritative);
    }

    /** The state of the synchronization lifecycle for a specific model. */
    @IntDef({
        SynchronizerState.START,
        SynchronizerState.MODEL_PENDING,
        SynchronizerState.RESTORING,
        SynchronizerState.TRACKING,
        SynchronizerState.CANCELLED,
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

        /** After a restore has been cancelled. */
        int CANCELLED = 4;
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

        /**
         * Called when the CombinedTabRestorer has finished or cancelled restoring all tabs for this
         * model.
         */
        void onRestoreFinished();

        /** Called when the CombinedTabRestorer has cancelled restoring all tabs for this model. */
        void onRestoreCancelled();

        /** Destroys the synchronizer and resets the manager state. */
        void reset();
    }

    private final String mWindowTag;
    private final PersistentStoreMigrationManager mMigrationManager;
    private final TabModelSelector mTabModelSelector;
    private final boolean mIsAuthoritative;
    private final Map<Token, Boolean> mGroupIncognitoStatus = new HashMap<>();
    private final IncognitoTabModelObserver mIncognitoTabModelObserver =
            new IncognitoTabModelObserver() {
                @Override
                public void onIncognitoModelCreated() {
                    assumeNonNull(mIncognitoSynchronizerManager).onModelCreated();
                }

                @Override
                public void didBecomeEmpty() {
                    assumeNonNull(mIncognitoSynchronizerManager).reset();
                }
            };

    private @Nullable StorageCollectionSynchronizer mIncognitoSynchronizer;
    private @Nullable StorageCollectionSynchronizer mRegularSynchronizer;
    private @Nullable CollectionSaveForwarder mRegularWindowForwarder;
    private @Nullable CollectionSaveForwarder mIncognitoWindowForwarder;
    private boolean mLoadIncognitoTabsOnStart;

    private final SynchronizerManager mRegularSynchronizerManager =
            new RegularSynchronizerManager();
    private final @Nullable SynchronizerManager mIncognitoSynchronizerManager;

    private final Callback<@Nullable Tab> mRegularActiveTabObserver =
            this::onRegularActiveTabChange;
    private final Callback<@Nullable Tab> mIncognitoActiveTabObserver =
            this::onIncognitoActiveTabChange;

    private final TabGroupModelFilterObserver mVisualDataUpdateObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                    Token groupId = destinationTab.getTabGroupId();
                    assert groupId != null;
                    mGroupIncognitoStatus.put(groupId, filter.getTabModel().isOffTheRecord());
                }

                @Override
                public void didRemoveTabGroup(
                        int oldRootId, @Nullable Token oldTabGroupId, int removalReason) {
                    if (oldTabGroupId == null) return;
                    mGroupIncognitoStatus.remove(oldTabGroupId);
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
     * @param windowTag The window tag to use for the window.
     * @param migrationManager The migration manager for the window.
     * @param tabModelSelector The {@link TabModelSelector} to observe changes for.
     * @param hasCipherFactory Whether a cipher factory was provided for OTR data.
     * @param isAuthoritative Whether this store is the authoritative store for the window.
     */
    public ModelTrackingOrchestrator(
            String windowTag,
            PersistentStoreMigrationManager migrationManager,
            TabModelSelector tabModelSelector,
            boolean hasCipherFactory,
            boolean isAuthoritative) {
        mWindowTag = windowTag;
        mMigrationManager = migrationManager;
        mTabModelSelector = tabModelSelector;
        mIsAuthoritative = isAuthoritative;

        if (hasCipherFactory) {
            mIncognitoSynchronizerManager = new IncognitoSynchronizerManager();

            TabModel incognitoModel = tabModelSelector.getModel(true);
            if (incognitoModel instanceof IncognitoTabModel itm) {
                itm.addIncognitoObserver(mIncognitoTabModelObserver);
            }
        } else {
            mIncognitoSynchronizerManager = null;
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
        if (mIsAuthoritative) {
            TabGroupVisualDataStore.cacheGroups(data.getGroupsData());
        }

        if (incognito) {
            if (mIncognitoSynchronizerManager != null) {
                mIncognitoSynchronizerManager.onDataLoaded(data);
            }
        } else {
            mRegularSynchronizerManager.onDataLoaded(data);
        }
    }

    /**
     * Called when the {@link CombinedTabRestorer} has finished restoring all tabs for one model.
     *
     * @param incognito Whether the model is incognito.
     */
    public void onRestoredForModel(boolean incognito) {
        if (!incognito) {
            mRegularSynchronizerManager.onRestoreFinished();
        } else if (mIncognitoSynchronizerManager != null) {
            mIncognitoSynchronizerManager.onRestoreFinished();
        }
    }

    /** Called when the {@link CombinedTabRestorer} has been cancelled for both models. */
    public void onRestoreCancelled() {
        mRegularSynchronizerManager.onRestoreCancelled();
        if (mIncognitoSynchronizerManager != null) {
            mIncognitoSynchronizerManager.onRestoreCancelled();
        }
    }

    /**
     * Called when the {@link CombinedTabRestorer} has finished restoring all tabs for both models.
     */
    public void onRestoreFinished() {
        if (!mIsAuthoritative) {
            onRestoredForModel(/* incognito= */ false);
            onRestoredForModel(/* incognito= */ true);
            return;
        }

        Callback<TabModel> clearUnusedNodesForModel = this::clearUnusedNodesForModel;
        ChainedTasks tasks = new ChainedTasks();
        for (TabModel model : mTabModelSelector.getModels()) {
            tasks.add(TaskTraits.UI_DEFAULT, clearUnusedNodesForModel.bind(model));
        }
        tasks.start(/* coalesceTasks= */ false);
    }

    /** Performs the cleanup required for the synchronizers when the TabStateStore is destroyed. */
    public void destroy() {
        TabModel incognitoModel = mTabModelSelector.getModel(true);
        if (mIncognitoSynchronizerManager != null
                && incognitoModel instanceof IncognitoTabModel itm) {
            itm.removeIncognitoObserver(mIncognitoTabModelObserver);
        }

        mGroupIncognitoStatus.clear();

        for (boolean incognito : new boolean[] {false, true}) {
            TabGroupModelFilter filter = getFilter(incognito);
            if (filter != null) {
                filter.removeTabGroupObserver(mVisualDataUpdateObserver);
            }
        }

        mRegularSynchronizerManager.reset();
        if (mIncognitoSynchronizerManager != null) mIncognitoSynchronizerManager.reset();
    }

    /** Saves the tab through an associated synchronizer. */
    public void saveTab(Tab tab) {
        StorageCollectionSynchronizer synchronizer =
                tab.isOffTheRecord() ? mIncognitoSynchronizer : mRegularSynchronizer;
        if (synchronizer == null) return;
        synchronizer.saveTab(tab);
    }

    /**
     * Whether the synchronizer for the given model is present.
     *
     * @param incognito Whether the synchronizer is for an incognito model.
     */
    public boolean isSynchronizerPresent(boolean incognito) {
        StorageCollectionSynchronizer synchronizer =
                incognito ? mIncognitoSynchronizer : mRegularSynchronizer;
        return synchronizer != null;
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
        assert !mIsAuthoritative;

        Profile profile = mTabModelSelector.getModel(incognito).getProfile();
        if (profile == null) return;

        try (ScopedStorageBatch ignored = createBatch(profile)) {
            var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
            getSynchronizer(profileAndCollection, incognito).fullSave();
            mMigrationManager.onShadowStoreCaughtUp();
        }

        initializeTrackingSuite(incognito);
    }

    private void cancelRestore(boolean incognito) {
        assert mIsAuthoritative;

        Profile profile = mTabModelSelector.getModel(incognito).getProfile();
        if (profile == null) return;

        var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
        getSynchronizer(profileAndCollection, incognito).cancelRestore();
    }

    private void saveTabGroupPayload(Token tabGroupId) {
        Boolean isIncognito = mGroupIncognitoStatus.get(tabGroupId);
        if (isIncognito == null) return;

        StorageCollectionSynchronizer synchronizer =
                isIncognito ? mIncognitoSynchronizer : mRegularSynchronizer;

        if (synchronizer != null) {
            synchronizer.saveTabGroupPayload(tabGroupId);
        }
    }

    private void initActiveTabTracking(boolean incognito) {
        var profileAndCollection = getProfileAndCollection(mTabModelSelector, incognito);
        if (incognito) {
            mIncognitoWindowForwarder =
                    CollectionSaveForwarder.createForTabStripCollection(
                            profileAndCollection.profile, profileAndCollection.collection);
        } else {
            mRegularWindowForwarder =
                    CollectionSaveForwarder.createForTabStripCollection(
                            profileAndCollection.profile, profileAndCollection.collection);
        }
        Callback<@Nullable Tab> obs =
                incognito ? mIncognitoActiveTabObserver : mRegularActiveTabObserver;
        mTabModelSelector
                .getModel(incognito)
                .getCurrentTabSupplier()
                .addSyncObserverAndPostIfNonNull(obs);
    }

    private void initVisualDataTracking(boolean incognito) {
        TabGroupModelFilter filter = getFilter(incognito);
        assert filter != null;

        // Add forwarders for untracked groups.
        for (Token groupId : filter.getAllTabGroupIds()) {
            mGroupIncognitoStatus.put(groupId, incognito);
        }

        filter.addTabGroupObserver(mVisualDataUpdateObserver);
    }

    private void clearUnusedNodesForModel(TabModel model) {
        Profile profile = model.getProfile();
        if (profile == null) return;

        TabStateStorageService service = TabStateStorageServiceFactory.getForProfile(profile);
        if (service == null) return;

        service.clearUnusedNodesForWindow(
                mWindowTag, model.isOffTheRecord(), model.getTabStripCollection());
    }

    private @Nullable TabGroupModelFilter getFilter(boolean incognito) {
        return mTabModelSelector.getTabGroupModelFilter(incognito);
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

    private void initializeTrackingSuite(boolean incognito) {
        initVisualDataTracking(incognito);
        initActiveTabTracking(incognito);
        initCollectionTracking(incognito);
    }

    private void onIncognitoActiveTabChange(@Nullable Tab ignored) {
        if (mIncognitoWindowForwarder == null) return;
        mIncognitoWindowForwarder.savePayload();
    }

    private void onRegularActiveTabChange(@Nullable Tab ignored) {
        if (mRegularWindowForwarder == null) return;
        mRegularWindowForwarder.savePayload();
    }

    private void cleanActiveTabTracking(boolean incognito) {
        TabModel model = mTabModelSelector.getModel(incognito);
        if (incognito) {
            if (mIncognitoWindowForwarder != null) {
                model.getCurrentTabSupplier().removeObserver(mIncognitoActiveTabObserver);
                mIncognitoWindowForwarder.destroy();
                mIncognitoWindowForwarder = null;
            }
        } else {
            if (mRegularWindowForwarder != null) {
                model.getCurrentTabSupplier().removeObserver(mRegularActiveTabObserver);
                mRegularWindowForwarder.destroy();
                mRegularWindowForwarder = null;
            }
        }
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
            if (mIsAuthoritative && mState != SynchronizerState.CANCELLED) {
                assert mState == SynchronizerState.START;
                mState = SynchronizerState.RESTORING;
                initRestoreOrchestrator(data, /* incognito= */ false);
                initVisualDataTracking(/* incognito= */ false);
                initActiveTabTracking(/* incognito= */ false);
            }
        }

        @Override
        public void onRestoreFinished() {
            if (mState == SynchronizerState.CANCELLED) return;
            if (mIsAuthoritative) {
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
        public void onRestoreCancelled() {
            if (mState == SynchronizerState.START && mLoadIncognitoTabsOnStart) {
                cancelRestore(/* incognito= */ false);
                mState = SynchronizerState.CANCELLED;
            }
        }

        @Override
        public void reset() {
            if (mRegularSynchronizer != null) {
                mRegularSynchronizer.destroy();
                mRegularSynchronizer = null;
            }
            mState = SynchronizerState.START;
            cleanActiveTabTracking(/* incognito= */ false);
        }
    }

    /** Manages the Synchronizer for the Incognito {@link TabModel}. */
    private class IncognitoSynchronizerManager implements SynchronizerManager {
        private @SynchronizerState int mState = SynchronizerState.START;
        private @Nullable Runnable mInitRestoreOrchestratorCallback;

        @Override
        public void onDataLoaded(StorageLoadedData data) {
            if (mIsAuthoritative && mState != SynchronizerState.CANCELLED) {
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
            if (mIsAuthoritative) {
                if (mState == SynchronizerState.MODEL_PENDING) {
                    mState = SynchronizerState.RESTORING;
                    assumeNonNull(mInitRestoreOrchestratorCallback).run();
                    initVisualDataTracking(/* incognito= */ true);
                    initActiveTabTracking(/* incognito= */ true);
                    mInitRestoreOrchestratorCallback = null;
                } else if (mState == SynchronizerState.START) {
                    mState = SynchronizerState.TRACKING;
                    initializeTrackingSuite(/* incognito= */ true);
                }
            } else if (mState == SynchronizerState.START && !mLoadIncognitoTabsOnStart) {
                mState = SynchronizerState.TRACKING;
                initializeTrackingSuite(/* incognito= */ true);
            }
        }

        @Override
        public void onRestoreFinished() {
            if (mState == SynchronizerState.CANCELLED) return;
            if (mIsAuthoritative) {
                if (mState == SynchronizerState.RESTORING) {
                    mState = SynchronizerState.TRACKING;
                    initCollectionTracking(/* incognito= */ true);
                }
            } else if (mState == SynchronizerState.START && mLoadIncognitoTabsOnStart) {
                fullSaveAndInitTracking(/* incognito= */ true);
                mState = SynchronizerState.TRACKING;
            }
            mLoadIncognitoTabsOnStart = false;
        }

        @Override
        public void onRestoreCancelled() {
            if (mState == SynchronizerState.START && mLoadIncognitoTabsOnStart) {
                cancelRestore(/* incognito= */ true);
                mState = SynchronizerState.CANCELLED;
            }
            mLoadIncognitoTabsOnStart = false;
        }

        @Override
        public void reset() {
            if (mIncognitoSynchronizer != null) {
                mIncognitoSynchronizer.destroy();
                mIncognitoSynchronizer = null;
            }

            if (mState != SynchronizerState.CANCELLED) {
                mLoadIncognitoTabsOnStart = false;
            }

            mState = SynchronizerState.START;
            mInitRestoreOrchestratorCallback = null;

            cleanActiveTabTracking(/* incognito= */ true);
        }
    }
}
