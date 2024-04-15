// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncController.TabCreationDelegate;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;

/**
 * Observes {@link TabGroupSyncService} for any incoming tab group updates from sync. Forwards the
 * updates to {@link LocalTabGroupMutationHelper} which does the actual updates to the tab model.
 * Additionally manages disabling and enabling local observers to avoid looping updates back to
 * sync.
 */
public final class TabGroupSyncRemoteObserver implements TabGroupSyncService.Observer {
    private final TabGroupModelFilter mTabGroupModelFilter;
    private final TabGroupSyncService mTabGroupSyncService;
    private final LocalTabGroupMutationHelper mLocalTabGroupMutationHelper;
    private final TabCreationDelegate mTabCreationDelegate;
    private final NavigationTracker mNavigationTracker;
    private final Callback<Boolean> mEnableLocalObserverCallback;
    private final Runnable mOnSyncInitializedCallback;

    /**
     * Constructor.
     *
     * @param tabGroupModelFilter The associated local {@link TabGroupModelFilter} to mutate for
     *     remote updates.
     * @param tabGroupSyncService The sync backend to observe.
     * @param localTabGroupMutationHelper Helper class for mutation of local tab model and groups.
     * @param tabCreationDelegate Helper class for with tab creation.
     * @param navigationTracker Tracker tracking navigations initiated by sync.
     * @param enableLocalObserverCallback Callback to enable/disable local observation.
     * @param onSyncInitializedCallback Callback to be notified about sync backend initialization.
     */
    public TabGroupSyncRemoteObserver(
            TabGroupModelFilter tabGroupModelFilter,
            TabGroupSyncService tabGroupSyncService,
            LocalTabGroupMutationHelper localTabGroupMutationHelper,
            TabCreationDelegate tabCreationDelegate,
            NavigationTracker navigationTracker,
            Callback<Boolean> enableLocalObserverCallback,
            Runnable onSyncInitializedCallback) {
        mTabGroupModelFilter = tabGroupModelFilter;
        mTabGroupSyncService = tabGroupSyncService;
        mLocalTabGroupMutationHelper = localTabGroupMutationHelper;
        mTabCreationDelegate = tabCreationDelegate;
        mNavigationTracker = navigationTracker;
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
            // We don't have a better way to handle those than not auto-opening them.
            // TODO(b/334379081): Handle it better.
            return;
        }

        mEnableLocalObserverCallback.onResult(false);
        mLocalTabGroupMutationHelper.updateTabGroup(tabGroup);
        mEnableLocalObserverCallback.onResult(true);
    }

    @Override
    public void onTabGroupRemoved(int localId) {
        assert localId != -1;
        mEnableLocalObserverCallback.onResult(false);
        List<Tab> tabs = mTabGroupModelFilter.getRelatedTabList(localId);
        getTabModel().closeMultipleTabs(tabs, /* canUndo= */ false);
        mEnableLocalObserverCallback.onResult(true);
    }

    @Override
    public void onTabGroupRemoved(String syncId) {}

    private TabModel getTabModel() {
        return mTabGroupModelFilter.getTabModel();
    }
}
