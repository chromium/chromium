// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.SystemClock;

import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupVisualDataStore;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabStripCollection;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Orchestrates saving of tabs to the {@link TabStateStorageService}. */
@NullMarked
public class TabStateStore implements TabPersistentStore {
    private static final String TAG = "TabStateStore";
    private static final int RESTORE_BATCH_SIZE = 5;

    private final TabStateStorageService mTabStateStorageService;
    private final TabCreatorManager mTabCreatorManager;
    private final TabModelSelector mTabModelSelector;
    private final TabStateAttributes.Observer mAttributesObserver =
            this::onTabStateDirtinessChanged;
    private final ObserverList<TabPersistentStoreObserver> mObservers = new ObserverList<>();
    private final Map<Token, CollectionSaveForwarder> mGroupForwarderMap = new HashMap<>();

    private @Nullable TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private @Nullable TabMoveObserver mTabMoveObserver;
    private @Nullable TabGroupModelFilter mFilter;
    private int mRestoredTabCount;

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

    private class TabMoveObserver implements TabModelObserver {
        private final TabModel mTabModel;

        private TabMoveObserver(TabModel tabModel) {
            mTabModel = tabModel;
            mTabModel.addObserver(this);
        }

        private void destroy() {
            mTabModel.removeObserver(this);
        }

        @Override
        public void didMoveTab(Tab tab, int newIndex, int curIndex) {
            onMoveTab(mTabModel, newIndex, curIndex);
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
                                    destinationTab.getProfile(),
                                    groupId,
                                    collection);
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
                    saveTabGroup(tabGroupId);
                }

                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    saveTabGroup(tabGroupId);
                }

                @Override
                public void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {
                    saveTabGroup(tabGroupId);
                }
            };

    /**
     * @param tabStateStorageService The {@link TabStateStorageService} to save to.
     * @param tabModelSelector The {@link TabModelSelector} to observe changes in. Regardless of the
     *     mode this store is in, this will be the real selector with real models. This should be
     *     treated as a read only object, no modifications should go through it.
     * @param tabCreatorManager Used to create new tabs on initial load. This may return real
     *     creators, or faked out creators if in non-authoritative mode.
     */
    public TabStateStore(
            TabStateStorageService tabStateStorageService,
            TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager) {
        mTabStateStorageService = tabStateStorageService;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
    }

    @Override
    public void onNativeLibraryReady() {
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            catchUpAndBeginTracking();
        }
    }

    private void catchUpAndBeginTracking() {
        assert mTabRegistrationObserver == null && mTabMoveObserver == null;
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                new InnerRegistrationObserver());

        mTabMoveObserver = new TabMoveObserver(mTabModelSelector.getModel(/* incognito= */ false));

        mFilter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(/* isIncognito= */ false);

        initVisualDataTracking();

        // TODO(https://crbug.com/451614469): Watch for incognito as well eventually. But before
        // things are fully functional, do not write any incognito data to avoid regressing on
        // privacy.
    }

    @Override
    public void waitForMigrationToFinish() {
        // Not relevant for this impl.
    }

    @Override
    public void saveState() {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void loadState(boolean ignoreIncognitoFiles) {
        loadAllTabsFromService();
    }

    @Override
    public void mergeState() {
        // Not currently supported by this impl.
        assert false;
    }

    @Override
    public void restoreTabs(boolean setActiveTab) {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void restoreTabStateForUrl(String url) {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void restoreTabStateForId(int id) {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public int getRestoredTabCount() {
        return mRestoredTabCount;
    }

    @Override
    public void clearState() {
        mTabStateStorageService.clearState();
    }

    @Override
    public void cancelLoadingTabs(boolean incognito) {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void removeTabFromQueues(Tab tab) {
        // Not relevant to impl.
    }

    @Override
    public void destroy() {
        if (mTabRegistrationObserver != null) {
            mTabRegistrationObserver.destroy();
        }
        if (mTabMoveObserver != null) {
            mTabMoveObserver.destroy();
        }

        for (CollectionSaveForwarder forwarder : mGroupForwarderMap.values()) {
            forwarder.destroy();
        }
        if (mFilter != null) {
            mFilter.removeTabGroupObserver(mVisualDataUpdateObserver);
        }
    }

    @Override
    public void saveTabListAsynchronously() {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void pauseSaveTabList() {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void resumeSaveTabList() {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void resumeSaveTabList(Runnable onSaveTabListRunnable) {
        // TODO(https://crbug.com/448151052): Implement.
    }

    @Override
    public void cleanupStateFile(int windowId) {
        // TODO(https://crbug.com/451624258): Implement.
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

    private void saveTab(Tab tab) {
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
        assumeNonNull(TabStateAttributes.from(tab)).removeObserver(mAttributesObserver);
        // TODO(https://crbug.com/430996004): Delete the tab record.
    }

    private void onMoveTab(TabModel tabModel, int newIndex, int curIndex) {
        // TODO(https://crbug.com/427254267): Add some sort of debouncing to avoid duplicate
        // and/or redundant saves when an operation with multiple events/moves.
        // TODO(https://crbug.com/427254267): A collections implementation will need pinned
        // and unpinned collections, but this is at the wrong scope to know about that.
        int start = Math.max(0, Math.min(newIndex, curIndex));
        int end = Math.min(tabModel.getCount() - 1, Math.max(newIndex, curIndex));
        Set<Token> tabGroupsToSave = new HashSet<>();
        for (int i = start; i <= end; i++) {
            Tab child = tabModel.getTabAt(i);
            Token groupId = child == null ? null : child.getTabGroupId();
            if (groupId != null) {
                tabGroupsToSave.add(groupId);
            }
        }
        for (Token groupId : tabGroupsToSave) {
            // TODO(https://crbug.com/427254267): Save the tab group's children index list.

            // Useless call to avoid compiler complaining until actually used.
            groupId.toBundle();
        }
        // TODO(https://crbug.com/427254267): Save the tab model's children index list.
    }

    private void loadAllTabsFromService() {
        long loadStartTime = SystemClock.elapsedRealtime();
        mTabStateStorageService.loadAllData(data -> onDataLoaded(data, loadStartTime));
    }

    private void onDataLoaded(StorageLoadedData data, long loadStartTime) {
        LoadedTabState[] loadedTabStates = data.getLoadedTabStates();

        long duration = SystemClock.elapsedRealtime() - loadStartTime;
        RecordHistogram.recordTimesHistogram("Tabs.TabStateStore.LoadAllTabsDuration", duration);

        mRestoredTabCount = loadedTabStates.length;
        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onInitialized(mRestoredTabCount);
        }

        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            TabGroupVisualDataStore.cacheGroups(data.getGroupsData());
        }

        // TODO(crbug.com/457771677): Special case the first batch to restore only the selected tab
        // then restore the remainder in posted batches.
        restoreNextBatchOfTabs(data, /* startIndex= */ 0, /* batchSize= */ loadedTabStates.length);
    }

    /**
     * Restores tabs in range {@code [startIndex, startIndex + batchSize)} from {@code data} or
     * until data is exhausted. Will post a task to restore the next batch if there are more tabs to
     * restore otherwise will signal the end of restoration.
     *
     * @param data The data to restore tabs from.
     * @param startIndex The index of the first tab to restore.
     * @param batchSize The number of tabs to restore.
     */
    private void restoreNextBatchOfTabs(StorageLoadedData data, int startIndex, int batchSize) {
        LoadedTabState[] loadedTabStates = data.getLoadedTabStates();
        int endIndex = Math.min(startIndex + batchSize, loadedTabStates.length);

        for (int i = startIndex; i < endIndex; i++) {
            LoadedTabState loadedTabState = loadedTabStates[i];
            @TabId int tabId = loadedTabState.tabId;
            Tab tab = resolveTab(loadedTabState.tabState, tabId, i);
            loadedTabState.onTabCreationCallback.onResult(tab);

            if (tab == null) {
                continue;
            }

            // TODO(https://crbug.com/448151052): Correctly mark the selected tab as active.
            // TODO(https://crbug.com/451624258): This is the opposite order of creation and details
            // from how the previous implementation did it. Verify this doesn't break anything.
            for (TabPersistentStoreObserver observer : mObservers) {
                observer.onDetailsRead(
                        i,
                        tabId,
                        tab.getUrl().getSpec(),
                        /* isStandardActiveIndex= */ false,
                        /* isIncognitoActiveIndex= */ false,
                        /* isIncognito= */ false,
                        /* fromMerge= */ false);
            }
        }

        if (endIndex < loadedTabStates.length) {
            // TODO(crbug.com/457771677): This is currently unreachable as we just fake restoring
            // everything in one batch.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> restoreNextBatchOfTabs(data, endIndex, RESTORE_BATCH_SIZE));
        } else {
            // TODO(crbug.com/457771677): Consider posting this to prevent jank.
            onFinishedCreatingAllTabs(data);
        }
    }

    private void onFinishedCreatingAllTabs(StorageLoadedData data) {
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            TabGroupVisualDataStore.removeCachedGroups(data.getGroupsData());
        }

        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onStateLoaded();
        }

        if (!ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            // When we aren't the authoritative source we don't trust ourselves to be correct.
            // Raze the db and rebuild from the loaded tab state to ensure we are in a known good
            // state. This is a no-op if we are the authoritative source as there shouldn't be a
            // delta and if there is we need a less blunt mechanism to reconcile the difference.
            clearState();
            catchUpAndBeginTracking();
        }
        data.destroy();
    }

    private @Nullable Tab resolveTab(TabState tabState, @TabId int tabId, int index) {
        if (tabState.contentsState == null || tabState.contentsState.buffer().limit() <= 0) {
            return null;
        }

        return mTabCreatorManager
                .getTabCreator(/* incognito= */ false)
                .createFrozenTab(tabState, tabId, index);
    }

    private void saveTabGroup(Token tabGroupId) {
        CollectionSaveForwarder forwarder = mGroupForwarderMap.get(tabGroupId);
        if (forwarder == null) return;
        forwarder.save();
    }

    private void initVisualDataTracking() {
        if (mFilter == null) return;

        TabStripCollection collection = mFilter.getTabModel().getTabStripCollection();
        if (collection == null) return;

        Profile profile = mFilter.getTabModel().getProfile();
        if (profile == null) return;

        // Add forwarders for untracked groups.
        for (Token groupId : mFilter.getAllTabGroupIds()) {
            CollectionSaveForwarder forwarder =
                    CollectionSaveForwarder.createForTabGroup(profile, groupId, collection);
            mGroupForwarderMap.put(groupId, forwarder);
        }

        mFilter.addTabGroupObserver(mVisualDataUpdateObserver);
    }
}
