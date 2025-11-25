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
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.CollectionStorageObserverFactory;
import org.chromium.chrome.browser.tab.StorageCollectionSynchronizer;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.StorageRestoreOrchestratorFactory;
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
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabStripCollection;

import java.util.HashMap;
import java.util.Map;

/** Orchestrates saving of tabs to the {@link TabStateStorageService}. */
@NullMarked
public class TabStateStore implements TabPersistentStore {
    private static final String TAG = "TabStateStore";
    private static final int RESTORE_BATCH_SIZE = 5;

    private final TabStateStorageService mTabStateStorageService;
    private final TabCreatorManager mTabCreatorManager;
    private final TabModelSelector mTabModelSelector;
    private final String mWindowTag;
    private final TabStateAttributes.Observer mAttributesObserver =
            this::onTabStateDirtinessChanged;
    private final ObserverList<TabPersistentStoreObserver> mObservers = new ObserverList<>();
    private final Map<Token, CollectionSaveForwarder> mGroupForwarderMap = new HashMap<>();

    private @Nullable TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private @Nullable TabGroupModelFilter mFilter;
    private @Nullable StorageCollectionSynchronizer mSynchronizer;
    private int mRestoredTabCount;
    private boolean mIsDestroyed;

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
            TabCreatorManager tabCreatorManager) {
        mTabStateStorageService = tabStateStorageService;
        mTabModelSelector = tabModelSelector;
        mWindowTag = windowTag;
        mTabCreatorManager = tabCreatorManager;
    }

    @Override
    public void onNativeLibraryReady() {
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            catchUpAndBeginTracking();
        }
    }

    private void catchUpAndBeginTracking() {
        assert mTabRegistrationObserver == null;
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(mTabModelSelector);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                new InnerRegistrationObserver());

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
        assert !mIsDestroyed;
        mIsDestroyed = true;

        if (mTabRegistrationObserver != null) {
            mTabRegistrationObserver.destroy();
        }

        for (CollectionSaveForwarder forwarder : mGroupForwarderMap.values()) {
            forwarder.destroy();
        }
        if (mFilter != null) {
            mFilter.removeTabGroupObserver(mVisualDataUpdateObserver);
        }

        if (mSynchronizer != null) {
            mSynchronizer.destroy();
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

    private void loadAllTabsFromService() {
        long loadStartTime = SystemClock.elapsedRealtime();
        // TODO(crbug.com/458335579): Figure out incognito.
        mTabStateStorageService.loadAllData(
                mWindowTag, /* isOffTheRecord= */ false, data -> onDataLoaded(data, loadStartTime));
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
            initRestoreOrchestrator(data);
        }

        if (mRestoredTabCount == 0) {
            onFinishedCreatingAllTabs(data);
            return;
        }

        restoreActiveTab(data);
    }

    /**
     * Restores the active tab from {@code data}. Will post a task to restore the next batch if
     * there are more tabs to restore otherwise will signal the end of restoration.
     *
     * @param data The data to restore tabs from.
     */
    private void restoreActiveTab(StorageLoadedData data) {
        if (mIsDestroyed) {
            cleanupStorageLoadedData(data);
            return;
        }

        LoadedTabState[] loadedTabStates = data.getLoadedTabStates();
        assert loadedTabStates.length > 0;

        int activeTabIndex = data.getActiveTabIndex();
        int restoredActiveTabIndex =
                (activeTabIndex > TabModel.INVALID_TAB_INDEX
                                && activeTabIndex < loadedTabStates.length)
                        ? activeTabIndex
                        : 0;
        restoreTab(
                loadedTabStates[restoredActiveTabIndex],
                restoredActiveTabIndex,
                /* isIncognito= */ false,
                /* isActive= */ true);

        if (loadedTabStates.length == 1) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> onFinishedCreatingAllTabs(data));
            return;
        }
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        restoreNextBatchOfTabs(
                                data,
                                restoredActiveTabIndex,
                                /* startIndex= */ 0,
                                /* batchSize= */ RESTORE_BATCH_SIZE));
    }

    /**
     * Restores a single tab.
     *
     * @param loadedTabState The tab state to restore.
     * @param index The index of the tab to restore.
     * @param isIncognito Whether the tab is in incognito mode.
     * @param isActive Whether the tab is the active tab.
     */
    private void restoreTab(
            LoadedTabState loadedTabState, int index, boolean isIncognito, boolean isActive) {
        @TabId int tabId = loadedTabState.tabId;
        Tab tab = resolveTab(loadedTabState.tabState, tabId, index);
        if (tab == null) return;

        // TODO(https://crbug.com/451624258): This is the opposite order of creation and details
        // from how the previous implementation did it. Verify this doesn't break anything.
        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onDetailsRead(
                    index,
                    tabId,
                    tab.getUrl().getSpec(),
                    /* isStandardActiveIndex= */ !isIncognito && isActive,
                    /* isIncognitoActiveIndex= */ isIncognito && isActive,
                    /* isIncognito= */ isIncognito,
                    /* fromMerge= */ false);
        }
    }

    /**
     * Restores tabs in range {@code [startIndex, startIndex + batchSize)} from {@code data} or
     * until data is exhausted. Will post a task to restore the next batch if there are more tabs to
     * restore otherwise will signal the end of restoration.
     *
     * @param data The data to restore tabs from.
     * @param restoredActiveTabIndex The index of the active tab that was restored already.
     * @param startIndex The index of the first tab to restore.
     * @param batchSize The number of tabs to restore.
     */
    private void restoreNextBatchOfTabs(
            StorageLoadedData data, int restoredActiveTabIndex, int startIndex, int batchSize) {
        assert startIndex >= 0;
        assert batchSize > 0;
        if (mIsDestroyed) {
            cleanupStorageLoadedData(data);
            return;
        }

        LoadedTabState[] loadedTabStates = data.getLoadedTabStates();
        int endIndex = Math.min(startIndex + batchSize, loadedTabStates.length);

        for (int i = startIndex; i < endIndex; i++) {
            // Skip the active tab as it was already restored by {@link #restoreActiveTab}.
            if (i == restoredActiveTabIndex) continue;

            restoreTab(loadedTabStates[i], i, /* isIncognito= */ false, /* isActive= */ false);
        }

        if (endIndex < loadedTabStates.length) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () ->
                            restoreNextBatchOfTabs(
                                    data, restoredActiveTabIndex, endIndex, RESTORE_BATCH_SIZE));
        } else {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> onFinishedCreatingAllTabs(data));
        }
    }

    private void onFinishedCreatingAllTabs(StorageLoadedData data) {
        cleanupStorageLoadedData(data);
        data = null;

        if (mIsDestroyed) return;

        initCollectionTracking();

        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onStateLoaded();
        }

        if (!ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            catchUpAndBeginTracking();
        }
    }

    private void cleanupStorageLoadedData(StorageLoadedData data) {
        if (!ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            // When we aren't the authoritative source we don't trust ourselves to be correct.
            // Raze the db and rebuild from the loaded tab state to ensure we are in a known good
            // state. This is a no-op if we are the authoritative source as there shouldn't be a
            // delta and if there is we need a less blunt mechanism to reconcile the difference.
            clearState();
        }
        if (ChromeFeatureList.sTabStorageSqlitePrototypeAuthoritativeReadSource.getValue()) {
            TabGroupVisualDataStore.removeCachedGroups(data.getGroupsData());
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

    private void saveTabGroupPayload(Token tabGroupId) {
        CollectionSaveForwarder forwarder = mGroupForwarderMap.get(tabGroupId);
        if (forwarder == null) return;
        forwarder.savePayload();
    }

    private void initVisualDataTracking() {
        assert mFilter != null;

        TabStripCollection collection = mFilter.getTabModel().getTabStripCollection();
        assert collection != null;

        Profile profile = mFilter.getTabModel().getProfile();
        assert profile != null;

        // Add forwarders for untracked groups.
        for (Token groupId : mFilter.getAllTabGroupIds()) {
            CollectionSaveForwarder forwarder =
                    CollectionSaveForwarder.createForTabGroup(profile, groupId, collection);
            mGroupForwarderMap.put(groupId, forwarder);
        }

        mFilter.addTabGroupObserver(mVisualDataUpdateObserver);
    }

    @EnsuresNonNull("mSynchronizer")
    private void maybeInitSynchronizer() {
        if (mSynchronizer != null) return;

        // TODO(https://crbug.com/451614469): Watch for incognito as well, eventually.
        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);

        TabStripCollection tabStripCollection = tabModel.getTabStripCollection();
        assert tabStripCollection != null;

        Profile profile = tabModel.getProfile();
        assert profile != null;

        mSynchronizer = new StorageCollectionSynchronizer(profile, tabStripCollection);
    }

    private void initRestoreOrchestrator(StorageLoadedData data) {
        maybeInitSynchronizer();

        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);

        Profile profile = tabModel.getProfile();
        assert profile != null;

        TabStripCollection tabStripCollection = tabModel.getTabStripCollection();
        assert tabStripCollection != null;

        StorageRestoreOrchestratorFactory factory =
                new StorageRestoreOrchestratorFactory(profile, tabStripCollection, data);
        mSynchronizer.consumeRestoreOrchestratorFactory(factory);
    }

    private void initCollectionTracking() {
        maybeInitSynchronizer();

        TabModel tabModel = mTabModelSelector.getModel(/* incognito= */ false);

        Profile profile = tabModel.getProfile();
        assert profile != null;

        CollectionStorageObserverFactory factory = new CollectionStorageObserverFactory(profile);
        mSynchronizer.consumeCollectionObserverFactory(factory);
    }
}
