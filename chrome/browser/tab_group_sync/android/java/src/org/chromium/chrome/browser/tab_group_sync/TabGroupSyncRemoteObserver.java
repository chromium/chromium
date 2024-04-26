// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/**
 * Observes {@link TabGroupSyncService} for any incoming tab group updates from sync for the current
 * window. Forwards the updates to {@link LocalTabGroupMutationHelper} which does the actual updates
 * to the tab model. Additionally manages disabling and enabling local observers to avoid looping
 * updates back to sync. Updates for other windows are ignored.
 */
public final class TabGroupSyncRemoteObserver implements TabGroupSyncService.Observer {
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private final Callback<Boolean> mEnableLocalObserverCallback;
    private final Runnable mOnSyncInitializedCallback;

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The associated local {@link TabGroupModelFilter} to mutate for
     *     remote updates.
     * @param tabGroupSyncService The sync backend to observe.
     * @param localTabGroupMutationHelper Helper class for mutation of local tab model and groups.
     * @param enableLocalObserverCallback Callback to enable/disable local observation.
     * @param onSyncInitializedCallback Callback to be notified about sync backend initialization.
     */
    public TabGroupSyncRemoteObserver(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            LocalTabGroupMutationHelper localTabGroupMutationHelper,
            Callback<Boolean> enableLocalObserverCallback,
            Runnable onSyncInitializedCallback) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mLocalTabGroupMutationHelper = localTabGroupMutationHelper;
        mEnableLocalObserverCallback = enableLocalObserverCallback;
        mOnSyncInitializedCallback = onSyncInitializedCallback;

        // Start observing sync.
        mTabGroupSyncService.addObserver(this);
    }

    /** Called at destruction. */
    public void destroy() {
        mTabGroupSyncService.removeObserver(this);
    }

    @Override
    public void onInitialized() {
        mOnSyncInitializedCallback.run();
    }

    @Override
    public void onTabGroupAdded(SavedTabGroup tabGroup) {
        assert tabGroup.localId == null;
        mEnableLocalObserverCallback.onResult(false);
        mLocalTabGroupMutationHelper.createNewTabGroup(tabGroup);
        mEnableLocalObserverCallback.onResult(true);
    }

    @Override
    public void onTabGroupUpdated(SavedTabGroup tabGroup) {
        if (tabGroup.localId == null) {
            // This is the case where the tab model doesn't have the group open, but the backend was
            // already aware of the group. The group might have been closed. Ignore it.
            // There can still be some cases where we never created a local group due to a crash.
            // We could also not have a window when the update was received, such as only CCT
            // running.
            // We don't have a better way to handle those than not auto-opening them.
            // TODO(b/334379081): Handle it better. Maybe store if the group was explictly closed.
            return;
        }

        if (!TabGroupSyncUtils.isInCurrentWindow(mTabGroupModelFilter, tabGroup.localId)) return;

        mEnableLocalObserverCallback.onResult(false);
        mLocalTabGroupMutationHelper.updateTabGroup(tabGroup);
        mEnableLocalObserverCallback.onResult(true);
    }

    @Override
    public void onTabGroupRemoved(LocalTabGroupId localId) {
        assert localId != null;
        if (!TabGroupSyncUtils.isInCurrentWindow(mTabGroupModelFilter, localId)) return;

        mEnableLocalObserverCallback.onResult(false);
        mLocalTabGroupMutationHelper.closeTabGroup(localId);
        mEnableLocalObserverCallback.onResult(true);
    }

    @Override
    public void onTabGroupRemoved(String syncId) {}

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}
