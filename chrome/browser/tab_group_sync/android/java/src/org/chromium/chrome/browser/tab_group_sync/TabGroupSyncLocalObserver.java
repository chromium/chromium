// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.HashSet;
import java.util.List;
import java.util.Locale;

/**
 * Responsible for observing local tab model system and notifying sync about tab group changes, tab
 * changes, navigations etc. Also responsible for running the startup routine which 1. Observes the
 * startup completion signals from both local tab model and {@link TabGroupSyncService} and 2.
 * Modifies local tab model to ensure that both local and sync version of tab groups are equivalent.
 */
public final class TabGroupSyncLocalObserver {
    private static final String TAG = "TG.LocalObserver";
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final RemoteTabGroupMutationHelper mRemoteTabGroupMutationHelper;

    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final NavigationTracker mNavigationTracker;
    private final NavigationObserver mNavigationObserver;
    private final HashSet<Integer> mTabIdsSelectedInSession = new HashSet<>();
    private boolean mIsObserving;

    /**
     * Constructor.
     *
     * @param tabModelSelector The {@link TabModelSelector} to observe for local tab changes.
     * @param tabGroupModelFilter The {@link TabGroupModelFilter} to observe for local tab group
     *     changes.
     * @param tabGroupSyncService The sync backend to be notified of local changes.
     * @param remoteTabGroupMutationHelper Helper class for mutation of sync.
     * @param navigationTracker Tracker tracking navigations initiated by sync.
     */
    public TabGroupSyncLocalObserver(
            TabModelSelector tabModelSelector,
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            RemoteTabGroupMutationHelper remoteTabGroupMutationHelper,
            NavigationTracker navigationTracker) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mRemoteTabGroupMutationHelper = remoteTabGroupMutationHelper;
        mNavigationTracker = navigationTracker;

        // Start observing tab groups and tab model.
        mTabModelObserver = createTabModelObserver();
        mTabGroupModelFilterObserver = createTabGroupModelFilterObserver();
        mTabGroupModelFilter.addObserver(mTabModelObserver);
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);

        // Start observing navigations.
        mNavigationObserver =
                new NavigationObserver(tabModelSelector, mTabGroupSyncService, mNavigationTracker);
    }

    /** Called on destroy. */
    public void destroy() {
        mTabGroupModelFilter.removeTabGroupObserver(mTabGroupModelFilterObserver);
        mTabGroupModelFilter.removeObserver(mTabModelObserver);
    }

    /**
     * Called to enable or disable this observer. When disabled, none of the local changes will not
     * be propagated to sync. Typically invoked when chrome is in the middle of applying remote
     * updates to the local tab model.
     *
     * @param enable Whether to enable the observer.
     */
    public void enableObservers(boolean enable) {
        mIsObserving = enable;
        mNavigationObserver.enableObservers(enable);
    }

    private TabModelObserver createTabModelObserver() {
        return new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, int type, int creationState, boolean markedForSelection) {
                if (!mIsObserving || tab.getTabGroupId() == null) return;
                LogUtils.log(TAG, "didAddTab");

                mRemoteTabGroupMutationHelper.addTab(
                        TabGroupSyncUtils.getLocalTabGroupId(tab),
                        tab,
                        mTabGroupModelFilter.getIndexOfTabInGroup(tab));
            }

            @Override
            public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                if (!mIsObserving || tabs.isEmpty()) return;
                LogUtils.log(TAG, "onFinishingMultipleTabClosure, tabs# " + tabs.size());

                mRemoteTabGroupMutationHelper.handleMultipleTabClosure(tabs);
            }

            // This method is for metrics only!
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (!mTabGroupModelFilter.isTabInTabGroup(tab)) return;

                if (tab.getTabGroupId() == null) return;

                LocalTabGroupId localId = TabGroupSyncUtils.getLocalTabGroupId(tab);
                SavedTabGroup savedGroup = mTabGroupSyncService.getGroup(localId);
                if (savedGroup == null) return;

                mTabGroupSyncService.onTabSelected(localId, tab.getId());

                if (mTabGroupSyncService.isRemoteDevice(savedGroup.creatorCacheGuid)) {
                    RecordUserAction.record("TabGroups.Sync.SelectedTabInRemotelyCreatedGroup");
                } else {
                    RecordUserAction.record("TabGroups.Sync.SelectedTabInLocallyCreatedGroup");
                }

                SavedTabGroupTab savedTab = getSavedTab(savedGroup, tab.getId());
                boolean tabWasLastUsedRemotely =
                        savedTab != null
                                && mTabGroupSyncService.isRemoteDevice(
                                        savedGroup.lastUpdaterCacheGuid);
                if (tabWasLastUsedRemotely) {
                    int tabId = tab.getId();
                    boolean wasAdded = mTabIdsSelectedInSession.add(tabId);
                    if (wasAdded) {
                        RecordUserAction.record("MobileCrossDeviceTabJourney");
                        RecordUserAction.record(
                                "TabGroups.Sync.SelectedRemotelyUpdatedTabInSession");
                    }
                }
            }
        };
    }

    private TabGroupModelFilterObserver createTabGroupModelFilterObserver() {
        return new TabGroupModelFilterObserver() {
            @Override
            public void didChangeTabGroupColor(int rootId, int newColor) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didChangeTabGroupColor, rootId = " + rootId);
                updateVisualData(
                        TabGroupSyncUtils.getLocalTabGroupId(mTabGroupModelFilter, rootId));
            }

            @Override
            public void didChangeTabGroupTitle(int rootId, String newTitle) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didChangeTabGroupTitle, rootId = " + rootId);
                updateVisualData(
                        TabGroupSyncUtils.getLocalTabGroupId(mTabGroupModelFilter, rootId));
            }

            @Override
            public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {
                if (!mIsObserving) return;
                LogUtils.log(
                        TAG, "didMergeTabToGroup, selectedTabIdInGroup = " + selectedTabIdInGroup);

                LocalTabGroupId tabGroupRootId =
                        TabGroupSyncUtils.getLocalTabGroupId(
                                mTabGroupModelFilter, movedTab.getRootId());
                if (groupExistsInSync(tabGroupRootId)) {
                    int positionInGroup = mTabGroupModelFilter.getIndexOfTabInGroup(movedTab);
                    mRemoteTabGroupMutationHelper.addTab(tabGroupRootId, movedTab, positionInGroup);
                } else {
                    mRemoteTabGroupMutationHelper.createRemoteTabGroup(tabGroupRootId);
                }
            }

            @Override
            public void didMoveWithinGroup(
                    Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {
                if (!mIsObserving) return;
                LogUtils.log(
                        TAG,
                        "didMoveWithinGroup, tabModelOldIndex = "
                                + tabModelOldIndex
                                + ", tabModelNewIndex = "
                                + tabModelNewIndex);

                // The tab position was changed. Update sync.
                int positionInGroup = mTabGroupModelFilter.getIndexOfTabInGroup(movedTab);
                int rootId = movedTab.getRootId();
                LocalTabGroupId tabGroupId =
                        TabGroupSyncUtils.getLocalTabGroupId(mTabGroupModelFilter, rootId);
                Log.w(
                        TAG,
                        String.format(
                                Locale.getDefault(),
                                "movedTab positionInGroup %d out of %d",
                                positionInGroup,
                                mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)));
                mRemoteTabGroupMutationHelper.moveTab(
                        tabGroupId, movedTab.getId(), positionInGroup);
            }

            @Override
            public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didMoveTabOutOfGroup, prevFilterIndex = " + prevFilterIndex);

                // Remove tab from the synced group.
                Tab prevRoot = mTabGroupModelFilter.getTabAt(prevFilterIndex);
                assert prevRoot != null;
                LocalTabGroupId tabGroupId =
                        TabGroupSyncUtils.getLocalTabGroupId(
                                mTabGroupModelFilter, prevRoot.getRootId());
                if (tabGroupId == null) return;
                mRemoteTabGroupMutationHelper.removeTab(tabGroupId, movedTab.getId());
            }

            @Override
            public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didCreateNewGroup");
                LocalTabGroupId localTabGroupId =
                        TabGroupSyncUtils.getLocalTabGroupId(
                                mTabGroupModelFilter, destinationTab.getRootId());
                if (groupExistsInSync(localTabGroupId)) return;

                mRemoteTabGroupMutationHelper.createRemoteTabGroup(localTabGroupId);
            }

            @Override
            public void committedTabGroupClosure(Token tabGroupId, boolean wasHiding) {
                StringBuilder builder =
                        new StringBuilder("committedTabGroupClosure, tabGroupId = ")
                                .append(tabGroupId)
                                .append(" wasHiding = ")
                                .append(wasHiding);
                LogUtils.log(TAG, builder.toString());

                mRemoteTabGroupMutationHelper.handleCommittedTabGroupClosure(
                        new LocalTabGroupId(tabGroupId), wasHiding);
            }

            @Override
            public void didRemoveTabGroup(
                    int oldRootId,
                    @Nullable Token oldTabGroupId,
                    @DidRemoveTabGroupReason int removalReason) {
                LogUtils.log(TAG, "didRemoveTabGroup, oldRootId " + oldRootId);
                if (oldTabGroupId == null) return;

                LocalTabGroupId localTabGroupId = new LocalTabGroupId(oldTabGroupId);
                if (removalReason == DidRemoveTabGroupReason.MERGE
                        || removalReason == DidRemoveTabGroupReason.UNGROUP) {
                    mRemoteTabGroupMutationHelper.removeGroup(localTabGroupId);
                }
            }
        };
    }

    private void updateVisualData(LocalTabGroupId tabGroupId) {
        // During group creation from sync, we set the title and color before the group is actually
        // created. Hence, tab group ID could be null.
        if (tabGroupId == null) return;
        mRemoteTabGroupMutationHelper.updateVisualData(tabGroupId);
    }

    private boolean groupExistsInSync(LocalTabGroupId rootId) {
        return mTabGroupSyncService.getGroup(rootId) != null;
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }

    private SavedTabGroupTab getSavedTab(SavedTabGroup savedGroup, int tabId) {
        for (SavedTabGroupTab savedTab : savedGroup.savedTabs) {
            if (savedTab.localId != null && savedTab.localId == tabId) return savedTab;
        }
        return null;
    }
}
