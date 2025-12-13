// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.base.Log;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;

/**
 * Responsible for observing local tab model system and notifying sync about tab group changes, tab
 * changes, navigations etc. Also responsible for running the startup routine which 1. Observes the
 * startup completion signals from both local tab model and {@link TabGroupSyncService} and 2.
 * Modifies local tab model to ensure that both local and sync version of tab groups are equivalent.
 */
@NullMarked
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
                    Tab tab,
                    @TabLaunchType int type,
                    @TabCreationState int creationState,
                    boolean markedForSelection) {
                LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
                if (!mIsObserving || localTabGroupId == null) return;
                LogUtils.log(TAG, "didAddTab");

                mRemoteTabGroupMutationHelper.addTab(
                        localTabGroupId, tab, mTabGroupModelFilter.getIndexOfTabInGroup(tab));
            }

            @Override
            public void willCloseTab(Tab tab, boolean didCloseAlone) {
                if (!mIsObserving || tab.getTabGroupId() == null || !didCloseAlone) return;
                LogUtils.log(TAG, "willCloseTab");

                mRemoteTabGroupMutationHelper.handleWillCloseTabs(Collections.singletonList(tab));
            }

            @Override
            public void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {
                if (!mIsObserving || tabs.isEmpty()) return;
                LogUtils.log(TAG, "willCloseMultipleTabs, tabs# " + tabs.size());

                mRemoteTabGroupMutationHelper.handleWillCloseTabs(tabs);
            }

            @Override
            public void willCloseAllTabs(boolean incognito) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "willCloseAllTabs");

                mRemoteTabGroupMutationHelper.handleWillCloseTabs(
                        TabModelUtils.convertTabListToListOfTabs(
                                mTabGroupModelFilter.getTabModel()));
            }

            @Override
            public void onFinishingMultipleTabClosure(List<Tab> tabs, boolean canRestore) {
                if (!mIsObserving || tabs.isEmpty()) return;
                LogUtils.log(TAG, "onFinishingMultipleTabClosure, tabs# " + tabs.size());

                mRemoteTabGroupMutationHelper.handleDidCloseTabs(tabs);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (!mIsObserving || tab.getTabGroupId() == null) return;
                LogUtils.log(TAG, "tabClosureUndone");

                mRemoteTabGroupMutationHelper.handleTabClosureUndone(tab);
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                // Intentionally observing all tab selections regardless of their origin.
                LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);

                // We notify TabGroupSyncService of the currently selected tab regardless of
                // whether it's part of a tab group or not. The accurate tracking of currently
                // selected tab is required for the MessagingBackendService.
                mTabGroupSyncService.onTabSelected(localTabGroupId, tab.getId(), tab.getTitle());

                // The rest of the method is required for metrics only.
                if (localTabGroupId == null) return;
                SavedTabGroup savedGroup = mTabGroupSyncService.getGroup(localTabGroupId);
                if (savedGroup == null) return;

                String creatorCacheGuid = savedGroup.creatorCacheGuid;
                if (creatorCacheGuid != null
                        && mTabGroupSyncService.isRemoteDevice(creatorCacheGuid)) {
                    RecordUserAction.record("TabGroups.Sync.SelectedTabInRemotelyCreatedGroup");
                } else {
                    RecordUserAction.record("TabGroups.Sync.SelectedTabInLocallyCreatedGroup");
                }

                SavedTabGroupTab savedTab = getSavedTab(savedGroup, tab.getId());
                String lastUpdaterCacheGuid =
                        savedTab != null ? savedTab.lastUpdaterCacheGuid : null;
                boolean tabWasLastUsedRemotely =
                        lastUpdaterCacheGuid != null
                                && mTabGroupSyncService.isRemoteDevice(lastUpdaterCacheGuid);
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
            public void didChangeTabGroupColor(Token tabGroupId, @TabGroupColorId int newColor) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didChangeTabGroupColor, tabGroupId = " + tabGroupId);
                updateVisualData(
                        TabGroupSyncUtils.getLocalTabGroupId(mTabGroupModelFilter, tabGroupId));
            }

            @Override
            public void didChangeTabGroupTitle(Token tabGroupId, @Nullable String newTitle) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didChangeTabGroupTitle, tabGroupId = " + tabGroupId);
                updateVisualData(
                        TabGroupSyncUtils.getLocalTabGroupId(mTabGroupModelFilter, tabGroupId));
            }

            @Override
            public void didMergeTabToGroup(Tab movedTab, boolean isDestinationTab) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didMergeTabToGroup, rootId = " + movedTab.getRootId());

                LocalTabGroupId localTabGroupId =
                        assertNonNull(
                                TabGroupSyncUtils.getLocalTabGroupId(
                                        mTabGroupModelFilter, movedTab.getTabGroupId()));
                if (groupExistsInSync(localTabGroupId)) {
                    int positionInGroup = mTabGroupModelFilter.getIndexOfTabInGroup(movedTab);
                    mRemoteTabGroupMutationHelper.addTab(
                            localTabGroupId, movedTab, positionInGroup);
                } else {
                    mRemoteTabGroupMutationHelper.createRemoteTabGroup(localTabGroupId);
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
                LocalTabGroupId localTabGroupId =
                        assertNonNull(
                                TabGroupSyncUtils.getLocalTabGroupId(
                                        mTabGroupModelFilter, movedTab.getTabGroupId()));
                Log.w(
                        TAG,
                        String.format(
                                Locale.getDefault(),
                                "movedTab positionInGroup %d out of %d",
                                positionInGroup,
                                mTabGroupModelFilter.getTabCountForGroup(
                                        localTabGroupId.tabGroupId)));
                mRemoteTabGroupMutationHelper.moveTab(
                        localTabGroupId, movedTab.getId(), positionInGroup);
            }

            @Override
            public void willMoveTabOutOfGroup(Tab movedTab, @Nullable Token destinationTabGroupId) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "willMoveTabOutOfGroup, tab id = " + movedTab.getId());

                // Remove tab from the synced group.
                LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(movedTab);
                if (localTabGroupId == null) return;
                mRemoteTabGroupMutationHelper.removeTab(localTabGroupId, movedTab.getId());
            }

            @Override
            public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                if (!mIsObserving) return;
                LogUtils.log(TAG, "didCreateNewGroup");
                LocalTabGroupId localTabGroupId =
                        assertNonNull(
                                TabGroupSyncUtils.getLocalTabGroupId(
                                        mTabGroupModelFilter, destinationTab.getTabGroupId()));
                if (groupExistsInSync(localTabGroupId)) return;

                mRemoteTabGroupMutationHelper.createRemoteTabGroup(localTabGroupId);
            }

            @Override
            public void willCloseTabGroup(Token tabGroupId, boolean isHiding) {
                if (!mIsObserving) return;
                String builder =
                        "willCloseTabGroup, tabGroupId = "
                                + tabGroupId
                                + " wasHiding = "
                                + isHiding;
                LogUtils.log(TAG, builder.toString());

                mRemoteTabGroupMutationHelper.handleWillCloseTabGroup(
                        new LocalTabGroupId(tabGroupId), isHiding);
            }

            @Override
            public void didRemoveTabGroup(
                    int oldRootId,
                    @Nullable Token oldTabGroupId,
                    @DidRemoveTabGroupReason int removalReason) {
                if (!mIsObserving) return;
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

    private void updateVisualData(@Nullable LocalTabGroupId tabGroupId) {
        // During group creation from sync, we set the title and color before the group is actually
        // created. Hence, tab group ID could be null.
        if (tabGroupId == null) return;
        mRemoteTabGroupMutationHelper.updateVisualData(tabGroupId);
    }

    private boolean groupExistsInSync(LocalTabGroupId localTabGroupId) {
        return mTabGroupSyncService.getGroup(localTabGroupId) != null;
    }

    private @Nullable SavedTabGroupTab getSavedTab(SavedTabGroup savedGroup, int tabId) {
        for (SavedTabGroupTab savedTab : savedGroup.savedTabs) {
            if (savedTab.localId != null && savedTab.localId == tabId) return savedTab;
        }
        return null;
    }

    boolean hasAnyPendingTabGroupClosuresForTesting() {
        return mRemoteTabGroupMutationHelper.hasAnyPendingTabGroupClosuresForTesting();
    }
}
