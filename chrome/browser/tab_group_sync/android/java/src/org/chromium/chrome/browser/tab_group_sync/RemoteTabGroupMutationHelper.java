// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.Pair;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.ClosingSource;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Helper class to create a {@link SavedTabGroup} based on a local tab group. It's a wrapper around
 * {@link TabGroupSyncService} to help with invoking mutation methods.
 */
@NullMarked
public class RemoteTabGroupMutationHelper {
    private static final String TAG = "TG.RemoteMutation";

    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private final Map<LocalTabGroupId, PendingTabGroupClosure> mPendingTabGroupClosures =
            new HashMap<>();

    /**
     * Helper class to track a closing tab group to ensure undo and commit work correctly.
     *
     * <p>When a tab closure is undone normal events regarding tab lifecycle are not fired. For
     * example, didAddTab() is not called when a tab is added back, instead tabClosureUndone() is
     * called. Same for tab group creation events not being fired. Hence we need to observe the
     * events related to committing and undoing tab closures to determine the final outcome.
     *
     * <p>Records all tabs in a {@link SavedTabGroup} when the group is starting to close. The
     * {@link RemoteTabGroupMutationHelper} updates the state of this class each time a tab in the
     * group is committed or undone. After adding the tabs to this class it is checked to see if all
     * the tabs are handled. If all tabs are handled follow up changes are made to sync 1) if a
     * group was hiding and had any tabs restored we need to explicitly reconcile it with the sync
     * database to ensure the tab data layer and sync match, 2) if a group was hiding and completely
     * closed the local mapping is removed, 3) if a group is being deleted and is undone it needs to
     * be added back to sync entirely.
     */
    private static class PendingTabGroupClosure {
        private final boolean mWasHiding;
        private final Set<Integer> mUnhandledTabIds = new HashSet<>();
        private final Set<Tab> mRestoredTabs = new HashSet<>();

        /**
         * @param savedTabGroup The closing synced tab group.
         * @param isHiding Whether the group is hiding.
         */
        PendingTabGroupClosure(SavedTabGroup savedTabGroup, boolean isHiding) {
            mWasHiding = isHiding;
            for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
                mUnhandledTabIds.add(savedTab.localId);
            }
        }

        /** Returns whether the group was hiding when we started tracking it. */
        boolean wasHiding() {
            return mWasHiding;
        }

        /** Add a tab that was restored to the group. */
        void addRestoredTab(Tab tab) {
            mRestoredTabs.add(tab);
        }

        /** Mark a tab as having been handled. This means it was either committed or undone. */
        void markTabHandled(Tab tab) {
            mUnhandledTabIds.remove(tab.getId());
        }

        /** Whether any tab closures were undone. */
        boolean anyTabsRestored() {
            assert allTabsHandled();
            return !mRestoredTabs.isEmpty();
        }

        /** Whether all the tab closures this object is tracking have been handled. */
        boolean allTabsHandled() {
            return mUnhandledTabIds.isEmpty();
        }

        /** Check if the set of tabs that are restored is the same as {@code tabs}. */
        boolean restoredTabsAre(List<Tab> tabs) {
            return mRestoredTabs.equals(new HashSet<>(tabs));
        }
    }

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The local tab model.
     * @param tabGroupSyncService The sync backend.
     * @param localTabGroupMutationHelper Local mutation helper to reconcile groups on undo.
     */
    public RemoteTabGroupMutationHelper(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            LocalTabGroupMutationHelper localTabGroupMutationHelper) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mLocalTabGroupMutationHelper = localTabGroupMutationHelper;
    }

    /**
     * Creates a remote tab group corresponding to the given local tab group.
     *
     * @param groupId The ID of the local tab group.
     */
    public void createRemoteTabGroup(LocalTabGroupId groupId) {
        Token tabGroupId = groupId.tabGroupId;
        LogUtils.log(TAG, "createRemoteTabGroup, groupId = " + tabGroupId);
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.localId = groupId;
        savedTabGroup.title = mTabGroupModelFilter.getTabGroupTitle(tabGroupId);
        if (savedTabGroup.title == null) {
            savedTabGroup.title = new String();
        }
        savedTabGroup.color = mTabGroupModelFilter.getTabGroupColor(tabGroupId);
        if (savedTabGroup.color == TabGroupColorUtils.INVALID_COLOR_ID) {
            savedTabGroup.color = TabGroupColorId.GREY;
        }

        List<Tab> tabs = mTabGroupModelFilter.getTabsInGroup(groupId.tabGroupId);
        for (int position = 0; position < tabs.size(); position++) {
            Tab tab = tabs.get(position);
            SavedTabGroupTab savedTab = new SavedTabGroupTab();
            savedTab.localId = tab.getId();
            savedTab.syncGroupId = savedTabGroup.syncId;

            Pair<GURL, String> urlAndTitle =
                    TabGroupSyncUtils.getFilteredUrlAndTitle(tab.getUrl(), tab.getTitle());
            savedTab.url = urlAndTitle.first;
            savedTab.title = urlAndTitle.second;
            savedTab.position = position;
            savedTabGroup.savedTabs.add(savedTab);
        }

        mTabGroupSyncService.addGroup(savedTabGroup);
    }

    /**
     * Called to update the visual data of a remote tab group. Uses default values, if title or
     * color are still unset for the local tab group.
     *
     * @param groupId The ID the local tab group.
     */
    public void updateVisualData(LocalTabGroupId groupId) {
        Token tabGroupId = groupId.tabGroupId;
        String title = new String();
        @TabGroupColorId int color = TabGroupColorId.GREY;
        if (mTabGroupModelFilter.tabGroupExists(tabGroupId)) {
            String tmpTitle = mTabGroupModelFilter.getTabGroupTitle(tabGroupId);
            if (tmpTitle != null) {
                title = tmpTitle;
            }
            @TabGroupColorId int tmpColor = mTabGroupModelFilter.getTabGroupColor(tabGroupId);
            if (tmpColor != TabGroupColorUtils.INVALID_COLOR_ID) {
                color = tmpColor;
            }
        }

        mTabGroupSyncService.updateVisualData(groupId, title, color);
    }

    /**
     * Removes a tab group from sync.
     *
     * @param groupId The local tab group ID.
     */
    public void removeGroup(LocalTabGroupId groupId) {
        mTabGroupSyncService.removeGroup(groupId);
    }

    /**
     * Adds a tab to the synced tab group.
     *
     * @param tabGroupId The local id of the modified tab group.
     * @param tab The tab to add.
     * @param position The position to add the tab at.
     */
    public void addTab(LocalTabGroupId tabGroupId, Tab tab, int position) {
        Pair<GURL, String> urlAndTitle =
                TabGroupSyncUtils.getFilteredUrlAndTitle(tab.getUrl(), tab.getTitle());
        mTabGroupSyncService.addTab(
                tabGroupId, tab.getId(), urlAndTitle.second, urlAndTitle.first, position);
    }

    /**
     * Moves a tab to the specified position in the synced tab group.
     *
     * @param tabGroupId The local id of the modified tab group.
     * @param tabId The id of the tab to move.
     * @param newPosition The position of the tab.
     */
    public void moveTab(LocalTabGroupId tabGroupId, int tabId, int newPosition) {
        mTabGroupSyncService.moveTab(tabGroupId, tabId, newPosition);
    }

    /**
     * Removes a tab from the synced tab group.
     *
     * @param tabGroupId The local id of the modified tab group.
     * @param tabId The id of the tab to remove.
     */
    public void removeTab(LocalTabGroupId tabGroupId, int tabId) {
        mTabGroupSyncService.removeTab(tabGroupId, tabId);
    }

    /**
     * Updates tab ID mappings for a particular group.
     *
     * @param localGroupId The local ID of the tab group.
     */
    public void updateTabIdMappingsOnStartup(LocalTabGroupId localGroupId) {
        LogUtils.log(TAG, "updateTabIdMappingsOnStartup, localGroupId = " + localGroupId);
        // Update tab ID mapping for tabs in the group.
        SavedTabGroup group = mTabGroupSyncService.getGroup(localGroupId);
        if (group == null) return;

        List<Tab> tabs = mTabGroupModelFilter.getTabsInGroup(localGroupId.tabGroupId);
        // We just reconciled local state with sync. The tabs should match.
        assert tabs.size() == group.savedTabs.size()
                : "Local tab count doesn't match with remote : local #"
                        + tabs.size()
                        + " vs remote #"
                        + group.savedTabs.size();
        for (int i = 0; i < group.savedTabs.size() && i < tabs.size(); i++) {
            SavedTabGroupTab savedTab = group.savedTabs.get(i);
            mTabGroupSyncService.updateLocalTabId(
                    localGroupId, assertNonNull(savedTab.syncId), tabs.get(i).getId());
        }
    }

    /**
     * Handle a tab group starting to close.
     *
     * @param groupId The group ID being closed.
     * @param isHiding Whether the group is hiding instead of being deleted.
     */
    public void handleWillCloseTabGroup(LocalTabGroupId groupId, boolean isHiding) {
        @Nullable SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(groupId);
        if (savedTabGroup == null) return;

        // Track the tab group closure including all current tabs in the savedTabGroup.
        createPendingTabGroupClosure(groupId, savedTabGroup, isHiding);

        if (!isHiding) {
            // The tab group is deleted entirely so remove it from sync.
            LogUtils.log(TAG, "handleWillCloseTabGroup: deleted group");

            mTabGroupSyncService.removeLocalTabGroupMapping(groupId, ClosingSource.DELETED_BY_USER);
            mTabGroupSyncService.removeGroup(assertNonNull(savedTabGroup.syncId));
            RecordUserAction.record("TabGroups.Sync.LocalDeleted");
        }
    }

    /**
     * Handle one or more tabs being closed.
     *
     * @param tabs The list of tabs that are starting to close.
     */
    public void handleWillCloseTabs(List<Tab> tabs) {
        LazyOneshotSupplier<Set<Token>> tabGroupIds =
                mTabGroupModelFilter.getLazyAllTabGroupIds(
                        tabs, /* includePendingClosures= */ false);
        for (Tab tab : tabs) {
            LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
            if (localTabGroupId == null) continue;

            // If a tab group is being completely hidden we don't want to remove its tabs from sync.
            // This handles that case since isTabGroupHiding will be true. However, to prevent cases
            // where isTabGroupHiding might be set incorrectly we also check that the tab model does
            // not still contain any tabs for the tab group as that would indicate only a subset of
            // the group is being closed.
            if (mTabGroupModelFilter.isTabGroupHiding(localTabGroupId.tabGroupId)
                    && !assumeNonNull(tabGroupIds.get()).contains(localTabGroupId.tabGroupId)) {
                continue;
            }

            // Remaining tabs will be in a tab group, but the closure event is either:
            // 1. Only a subset of tabs in the group.
            // 2. The group is to be deleted from sync so removing the tabs from sync is ok.
            mTabGroupSyncService.removeTab(localTabGroupId, tab.getId());
        }
    }

    /**
     * Handle one or more tab closures being committed.
     *
     * @param tabs The list of tabs that are committed to closing.
     */
    public void handleDidCloseTabs(List<Tab> tabs) {
        for (Tab tab : tabs) {
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) continue;

            tryUpdatePendingGroupClosure(tab, /* isUndone= */ false);
            // Case: subset of tabs in group closed, action committed.
            // If tryUpdatePendingGroupClosure returns false we hit this case and don't need to do
            // anything else since handleWillCloseTabs() already removed the tabs from sync.
        }
    }

    /**
     * Called when a tab closure is undone.
     *
     * @param tab The tab that was restored.
     */
    public void handleTabClosureUndone(Tab tab) {
        LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
        if (localTabGroupId == null) return;

        if (!tryUpdatePendingGroupClosure(tab, /* isUndone= */ true)) {
            // Case: subset of tabs in group closed, action undone.
            // No pending group closure is associated with this event so we need to add the tab back
            // to its synced group.

            LogUtils.log(TAG, "handleTabClosureUndone: addBackToGroup");
            List<Tab> groupTabs = mTabGroupModelFilter.getTabsInGroup(localTabGroupId.tabGroupId);
            int position = groupTabs.indexOf(tab);
            addTab(localTabGroupId, tab, position);
        }
    }

    private void createPendingTabGroupClosure(
            LocalTabGroupId localTabGroupId, SavedTabGroup savedTabGroup, boolean isHiding) {
        PendingTabGroupClosure pendingClosure = new PendingTabGroupClosure(savedTabGroup, isHiding);
        // If the group is for some reason empty don't track it to avoid leaking objects.
        if (pendingClosure.allTabsHandled()) return;

        mPendingTabGroupClosures.put(localTabGroupId, pendingClosure);
    }

    private boolean tryUpdatePendingGroupClosure(Tab tab, boolean isUndone) {
        LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
        if (localTabGroupId == null) return false;

        @Nullable
        PendingTabGroupClosure pendingClosure = mPendingTabGroupClosures.get(localTabGroupId);

        // A subset of tabs in a tab group is either being committed or undone. Early out and let
        // the caller handle the change appropriately.
        if (pendingClosure == null) return false;

        // Update state tracking.
        pendingClosure.markTabHandled(tab);
        if (isUndone) {
            pendingClosure.addRestoredTab(tab);
        }

        // More tabs need to commit or be undone before we can do further processing.
        if (!pendingClosure.allTabsHandled()) return true;

        if (pendingClosure.anyTabsRestored()) {
            // We restored at least one tab so we need to make sure everything is up-to-date.
            if (pendingClosure.wasHiding()) {
                // Case: group hidden, action undone.
                LogUtils.log(
                        TAG, "tryUpdatePendingGroupClosure: hidden group restored posting update.");
                assert pendingClosure.restoredTabsAre(
                                mTabGroupModelFilter.getTabsInGroup(tab.getTabGroupId()))
                        : "Unexpected tabs restored.";

                // In the case the tab group was hiding it should still have a mapping. However, a
                // remote update might've come in while the group was closing that either deleted or
                // modified the group. If the group was deleted we should not recreate it. If the
                // group was modified we need to use `updateTabGroup` to catch it up to reflect the
                // state in sync. We cannot do this synchronously as this causes concurrent change
                // modification issues in PendingTabClosureManager. Instead post this work so it
                // happens after.
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            if (!mTabGroupModelFilter.tabGroupExists(localTabGroupId.tabGroupId)) {
                                return;
                            }
                            @Nullable SavedTabGroup savedGroup =
                                    mTabGroupSyncService.getGroup(localTabGroupId);
                            // Don't recreate the group if the group was deleted remotely.
                            if (savedGroup != null) {
                                LogUtils.log(
                                        TAG,
                                        "tryUpdatePendingGroupClosure: hidden group found in sync,"
                                                + " reconciling");
                                mLocalTabGroupMutationHelper.updateTabGroup(savedGroup);
                            }
                        });
            } else {
                // Case: group deleted, action undone.
                LogUtils.log(TAG, "tryUpdatePendingGroupClosure: deleted group restored.");
                // If we deleted the group locally and we undid it we just need to recreate the
                // group.
                // This does mean that if the group was open on another client it will be closed and
                // a new group created, but this is necessary to apply deletions/closures to sync as
                // soon as they occur.
                createRemoteTabGroup(localTabGroupId);
            }
        } else if (pendingClosure.wasHiding()) {
            // Case: Group hidden, action committed.
            LogUtils.log(TAG, "tryUpdatePendingGroupClosure: hidden group committed");
            // No tabs were restored, but the group is hiding. Now that it is no longer possible to
            // undo the operation we can drop the local mapping.
            RecordUserAction.record("TabGroups.Sync.LocalHidden");
            mTabGroupSyncService.removeLocalTabGroupMapping(
                    localTabGroupId, ClosingSource.CLOSED_BY_USER);
        }
        // else
        // Case: Group deleted, action committed.
        // We already handled that case in handleWillCloseTabGroup().

        mPendingTabGroupClosures.remove(localTabGroupId);
        return true;
    }

    boolean hasAnyPendingTabGroupClosuresForTesting() {
        return !mPendingTabGroupClosures.isEmpty(); // IN-TEST
    }
}
