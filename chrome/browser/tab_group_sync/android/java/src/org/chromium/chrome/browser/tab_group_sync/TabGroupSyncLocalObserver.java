// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;

/**
 * Responsible for observing local tab model system and notifying sync about tab group changes, tab
 * changes, navigations etc. Also responsible for running the startup routine which 1. Observes the
 * startup completion signals from both local tab model and {@link TabGroupSyncService} and 2.
 * Modifies local tab model to ensure that both local and sync version of tab groups are equivalent.
 */
public final class TabGroupSyncLocalObserver {
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final RemoteTabGroupMutationHelper mRemoteTabGroupMutationHelper;

    private final TabModelObserver mTabModelObserver;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final NavigationTracker mNavigationTracker;
    private final NavigationObserver mNavigationObserver;
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

                mRemoteTabGroupMutationHelper.addTab(
                        tab.getRootId(), tab, mTabGroupModelFilter.getIndexOfTabInGroup(tab));
            }

            @Override
            public void onFinishingMultipleTabClosure(List<Tab> tabs) {
                if (!mIsObserving || tabs.isEmpty()) return;

                // TODO(b/331466817): Differentiate between close group and close tab.
            }
        };
    }

    private TabGroupModelFilterObserver createTabGroupModelFilterObserver() {
        return new TabGroupModelFilterObserver() {
            @Override
            public void didChangeTabGroupColor(int rootId, int newColor) {
                updateVisualData(rootId);
            }

            @Override
            public void didChangeTabGroupTitle(int rootId, String newTitle) {
                updateVisualData(rootId);
            }

            @Override
            public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {
                if (!mIsObserving) return;

                int tabGroupRootId = movedTab.getRootId();
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
                // The tab position was changed. Update sync.
                int positionInGroup = mTabGroupModelFilter.getIndexOfTabInGroup(movedTab);
                mRemoteTabGroupMutationHelper.updateTab(
                        movedTab.getRootId(), movedTab, positionInGroup);
            }

            @Override
            public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {
                if (!mIsObserving) return;

                // Remove tab from the synced group.
                Tab prevRoot = getTabModel().getTabAt(prevFilterIndex);
                assert prevRoot != null;
                mRemoteTabGroupMutationHelper.removeTab(prevRoot.getRootId(), movedTab.getId());
            }

            @Override
            public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                if (!mIsObserving) return;
                if (groupExistsInSync(destinationTab.getRootId())) return;

                mRemoteTabGroupMutationHelper.createRemoteTabGroup(destinationTab.getRootId());
            }
        };
    }

    private void updateVisualData(int tabGroupId) {
        if (!mIsObserving) return;
        mRemoteTabGroupMutationHelper.updateVisualData(tabGroupId);
    }

    private boolean groupExistsInSync(int rootId) {
        return mTabGroupSyncService.getGroup(rootId) != null;
    }

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}
