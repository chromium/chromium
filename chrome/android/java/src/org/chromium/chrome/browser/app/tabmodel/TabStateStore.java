// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.SystemClock;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageService.LoadedTabState;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
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
     * @param tabModelSelector The {@link TabModelSelector} to observe changes in.
     * @param tabCreatorManager Used to create new tabs on initial load.
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
        // TODO(https://crbug.com/448151845): Raze the db.
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
        if (attributes.addObserver(mAttributesObserver) == DirtinessState.DIRTY) {
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
        mTabStateStorageService.loadAllTabs(
                (loadedTabStates) -> onTabsLoaded(loadedTabStates, loadStartTime));
    }

    private void onTabsLoaded(LoadedTabState[] loadedTabStates, long loadStartTime) {
        long duration = SystemClock.elapsedRealtime() - loadStartTime;
        Log.i(TAG, "Loaded %d tabs in %dms", loadedTabStates.length, duration);
        mRestoredTabCount = loadedTabStates.length;

        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onInitialized(mRestoredTabCount);
        }

        for (int i = 0; i < loadedTabStates.length; i++) {
            TabState tabState = loadedTabStates[i].tabState;
            if (tabState.contentsState == null || tabState.contentsState.buffer().limit() <= 0) {
                Log.i(TAG, " Tab %d: no state", i);
                loadedTabStates[i].onTabCreationCallback.onResult(null);
                continue;
            }

            @TabId int tabId = loadedTabStates[i].tabId;
            Tab tab =
                    mTabCreatorManager
                            .getTabCreator(/* incognito= */ false)
                            .createFrozenTab(tabState, tabId, i);
            loadedTabStates[i].onTabCreationCallback.onResult(tab);

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

        for (TabPersistentStoreObserver observer : mObservers) {
            observer.onStateLoaded();
        }
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
