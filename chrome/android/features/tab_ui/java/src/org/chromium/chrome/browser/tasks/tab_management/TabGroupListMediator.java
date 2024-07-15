// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;

/** Populates a {@link ModelList} with an item for each tab group. */
public class TabGroupListMediator {
    // Internal state enum to track where a group lives. It can either be in the current tab
    // model/window/activity, in the current activity and closing, in another one, or hidden.
    // Hidden means only the sync side know about it. Everything is assumed to be non-incognito.
    // In other tab models is difficult to work with, since often tha tab model is not even
    // loaded into memory. For currently closing groups we need to special case the behavior to
    // properly undo or commit the pending operations.
    @IntDef({
        TabGroupState.IN_CURRENT,
        TabGroupState.IN_CURRENT_CLOSING,
        TabGroupState.IN_ANOTHER,
        TabGroupState.HIDDEN,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface TabGroupState {
        int IN_CURRENT = 0;
        int IN_CURRENT_CLOSING = 1;
        int IN_ANOTHER = 2;
        int HIDDEN = 3;
    }

    private final ModelList mModelList;
    private final PropertyModel mPropertyModel;
    private final TabGroupModelFilter mFilter;
    private final BiConsumer<GURL, Callback<Drawable>> mFaviconResolver;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final SyncService mSyncService;
    private final CallbackController mCallbackController = new CallbackController();
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::repopulateModelList));

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void tabClosureUndone(Tab tab) {
                    // Sync events aren't sent when a tab closure is undone since sync doesn't know
                    // anything happened until the closure is committed. Make sure the UI is up to
                    // date (with the right TabGroupState) if an undo related to a tab group
                    // happens.
                    if (mFilter.isTabInTabGroup(tab)) {
                        mPendingRefresh.post();
                    }
                }
            };

    private final TabGroupSyncService.Observer mTabGroupSyncObserver =
            new Observer() {
                @Override
                public void onInitialized() {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupRemoved(LocalTabGroupId localId, @TriggerSource int source) {
                    mPendingRefresh.post();
                }

                @Override
                public void onTabGroupRemoved(String syncId, @TriggerSource int source) {
                    mPendingRefresh.post();
                }
            };

    private final SyncService.SyncStateChangedListener mSyncStateChangeListener =
            new SyncService.SyncStateChangedListener() {
                @Override
                public void syncStateChanged() {
                    boolean enabled =
                            mSyncService.getActiveDataTypes().contains(ModelType.SAVED_TAB_GROUP);
                    mPropertyModel.set(TabGroupListProperties.SYNC_ENABLED, enabled);
                }
            };

    /**
     * @param modelList Side effect is adding items to this list.
     * @param propertyModel Properties for the empty state.
     * @param filter Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     * @param syncService Used to query active sync types.
     */
    public TabGroupListMediator(
            ModelList modelList,
            PropertyModel propertyModel,
            TabGroupModelFilter filter,
            BiConsumer<GURL, Callback<Drawable>> faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ActionConfirmationManager actionConfirmationManager,
            SyncService syncService) {
        mModelList = modelList;
        mPropertyModel = propertyModel;
        mFilter = filter;
        mFaviconResolver = faviconResolver;
        mTabGroupSyncService = tabGroupSyncService;
        mPaneManager = paneManager;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mActionConfirmationManager = actionConfirmationManager;
        mSyncService = syncService;

        mFilter.addObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }
        mSyncService.addSyncStateChangedListener(mSyncStateChangeListener);

        repopulateModelList();
        mSyncStateChangeListener.syncStateChanged();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        mFilter.removeObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
        }
        mSyncService.removeSyncStateChangedListener(mSyncStateChangeListener);
        mCallbackController.destroy();
    }

    private @TabGroupState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return TabGroupState.HIDDEN;
        }
        Token groupId = savedTabGroup.localId.tabGroupId;
        boolean isFullyClosing = true;
        int rootId = Tab.INVALID_TAB_ID;
        TabList tabList = mFilter.getTabModel().getComprehensiveModel();
        for (int i = 0; i < tabList.getCount(); i++) {
            Tab tab = tabList.getTabAt(i);
            if (groupId.equals(tab.getTabGroupId())) {
                rootId = tab.getRootId();
                isFullyClosing &= tab.isClosing();
            }
        }
        if (rootId == Tab.INVALID_TAB_ID) return TabGroupState.IN_ANOTHER;

        // If the group is only partially closing no special case is required since we still have to
        // do all the IN_CURRENT work and returning to the tab group via the dialog will work.
        return isFullyClosing ? TabGroupState.IN_CURRENT_CLOSING : TabGroupState.IN_CURRENT;
    }

    private List<SavedTabGroup> getSortedGroupList() {
        List<SavedTabGroup> groupList = new ArrayList<>();
        if (mTabGroupSyncService == null) return groupList;

        for (String syncGroupId : mTabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncGroupId);
            assert !savedTabGroup.savedTabs.isEmpty();

            // To simplify interactions, do not include any groups currently open in other windows.
            if (getState(savedTabGroup) != TabGroupState.IN_ANOTHER) {
                groupList.add(savedTabGroup);
            }
        }
        groupList.sort((a, b) -> Long.compare(b.creationTimeMs, a.creationTimeMs));
        return groupList;
    }

    private void repopulateModelList() {
        mModelList.clear();
        for (SavedTabGroup savedTabGroup : getSortedGroupList()) {
            PropertyModel model =
                    TabGroupRowMediator.buildModel(
                            savedTabGroup,
                            mFaviconResolver,
                            () -> openGroup(savedTabGroup),
                            () -> processDeleteGroup(savedTabGroup));

            ListItem listItem = new ListItem(0, model);
            mModelList.add(listItem);
        }

        boolean empty = mModelList.size() <= 0;
        mPropertyModel.set(TabGroupListProperties.EMPTY_STATE_VISIBLE, empty);
    }

    private void openGroup(SavedTabGroup savedTabGroup) {
        @TabGroupState int state = getState(savedTabGroup);
        if (state == TabGroupState.IN_ANOTHER) {
            return;
        }

        if (state == TabGroupState.HIDDEN) {
            RecordUserAction.record("SyncedTabGroup.OpenNewLocal");
        } else {
            RecordUserAction.record("SyncedTabGroup.OpenExistingLocal");
        }

        if (state == TabGroupState.IN_CURRENT_CLOSING) {
            for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
                if (savedTab.localId != null) {
                    mFilter.getTabModel().cancelTabClosure(savedTab.localId);
                }
            }
        } else if (state == TabGroupState.HIDDEN) {
            String syncId = savedTabGroup.syncId;
            mTabGroupUiActionHandler.openTabGroup(syncId);
            savedTabGroup = mTabGroupSyncService.getGroup(syncId);
            assert savedTabGroup.localId != null;
        }

        int rootId = mFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
        assert rootId != Tab.INVALID_TAB_ID;
        mPaneManager.focusPane(PaneId.TAB_SWITCHER);
        TabSwitcherPaneBase tabSwitcherPaneBase =
                (TabSwitcherPaneBase) mPaneManager.getPaneForId(PaneId.TAB_SWITCHER);
        boolean success = tabSwitcherPaneBase.requestOpenTabGroupDialog(rootId);
        assert success;
    }

    private void processDeleteGroup(SavedTabGroup savedTabGroup) {
        mActionConfirmationManager.processDeleteGroupAttempt(
                (@ConfirmationResult Integer result) -> {
                    if (result != ConfirmationResult.CONFIRMATION_NEGATIVE) {
                        deleteGroup(savedTabGroup);
                    }
                });
    }

    private void deleteGroup(SavedTabGroup savedTabGroup) {
        @TabGroupState int state = getState(savedTabGroup);
        if (state == TabGroupState.IN_ANOTHER) {
            return;
        }

        if (state == TabGroupState.HIDDEN) {
            RecordUserAction.record("SyncedTabGroup.DeleteWithoutLocal");
        } else {
            RecordUserAction.record("SyncedTabGroup.DeleteWithLocal");
        }

        if (state == TabGroupState.IN_CURRENT_CLOSING) {
            for (SavedTabGroupTab savedTab : savedTabGroup.savedTabs) {
                if (savedTab.localId != null) {
                    mFilter.getTabModel().commitTabClosure(savedTab.localId);
                }
            }
            // Because the pending closure might have been hiding or part of a closure containing
            // more tabs we need to forcibly remove the group.
            mTabGroupSyncService.removeGroup(savedTabGroup.syncId);
        } else if (state == TabGroupState.IN_CURRENT) {
            int rootId = mFilter.getRootIdFromStableId(savedTabGroup.localId.tabGroupId);
            List<Tab> tabsToClose = mFilter.getRelatedTabListForRootId(rootId);
            mFilter.closeMultipleTabs(
                    tabsToClose, /* canUndo= */ false, /* hideTabGroups= */ false);
        } else {
            mTabGroupSyncService.removeGroup(savedTabGroup.syncId);
        }
    }
}
