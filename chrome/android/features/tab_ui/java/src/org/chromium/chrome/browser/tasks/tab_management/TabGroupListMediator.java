// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Populates a {@link ModelList} with an item for each tab group. */
public class TabGroupListMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final PropertyModel mPropertyModel;
    private final TabGroupModelFilter mFilter;
    private final FaviconResolver mFaviconResolver;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @Nullable DataSharingService mDataSharingService;
    private final IdentityManager mIdentityManager;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final SyncService mSyncService;
    private final ModalDialogManager mModalDialogManager;
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

                @Override
                public void onTabGroupLocalIdChanged(
                        String syncTabGroupId, @Nullable LocalTabGroupId localTabGroupId) {
                    mPendingRefresh.post();
                }
            };

    private final SyncService.SyncStateChangedListener mSyncStateChangeListener =
            new SyncService.SyncStateChangedListener() {
                @Override
                public void syncStateChanged() {
                    boolean enabled =
                            mSyncService.getActiveDataTypes().contains(DataType.SAVED_TAB_GROUP);
                    mPropertyModel.set(TabGroupListProperties.SYNC_ENABLED, enabled);
                }
            };

    private final DataSharingService.Observer mDataSharingObserver =
            new DataSharingService.Observer() {
                @Override
                public void onGroupChanged(GroupData groupData) {
                    mPendingRefresh.post();
                }

                @Override
                public void onGroupAdded(GroupData groupData) {
                    mPendingRefresh.post();
                }

                @Override
                public void onGroupRemoved(String groupId) {
                    mPendingRefresh.post();
                }
            };

    /**
     * @param context Used to load resources and create views.
     * @param modelList Side effect is adding items to this list.
     * @param propertyModel Properties for the empty state.
     * @param filter Used to read current tab groups.
     * @param faviconResolver Used to fetch favicon images for some tabs.
     * @param tabGroupSyncService Used to fetch synced copy of tab groups.
     * @param dataSharingService Used to fetch shared group data.
     * @param identityManager Used to fetch current account information.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     * @param syncService Used to query active sync types.
     * @param modalDialogManager Used to show error dialogs.
     */
    public TabGroupListMediator(
            Context context,
            ModelList modelList,
            PropertyModel propertyModel,
            TabGroupModelFilter filter,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService,
            IdentityManager identityManager,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ActionConfirmationManager actionConfirmationManager,
            SyncService syncService,
            ModalDialogManager modalDialogManager) {
        mContext = context;
        mModelList = modelList;
        mPropertyModel = propertyModel;
        mFilter = filter;
        mFaviconResolver = faviconResolver;
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mIdentityManager = identityManager;
        mPaneManager = paneManager;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mActionConfirmationManager = actionConfirmationManager;
        mSyncService = syncService;
        mModalDialogManager = modalDialogManager;

        mFilter.addObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }
        if (mDataSharingService != null) {
            mDataSharingService.addObserver(mDataSharingObserver);
        }
        mSyncService.addSyncStateChangedListener(mSyncStateChangeListener);

        repopulateModelList();
        mSyncStateChangeListener.syncStateChanged();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        destroyAndClearAllRows();
        mFilter.removeObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
        }
        if (mDataSharingService != null) {
            mDataSharingService.removeObserver(mDataSharingObserver);
        }
        mSyncService.removeSyncStateChangedListener(mSyncStateChangeListener);
        mCallbackController.destroy();
    }

    private @GroupWindowState int getState(SavedTabGroup savedTabGroup) {
        if (savedTabGroup.localId == null) {
            return GroupWindowState.HIDDEN;
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
        if (rootId == Tab.INVALID_TAB_ID) return GroupWindowState.IN_ANOTHER;

        // If the group is only partially closing no special case is required since we still have to
        // do all the IN_CURRENT work and returning to the tab group via the dialog will work.
        return isFullyClosing ? GroupWindowState.IN_CURRENT_CLOSING : GroupWindowState.IN_CURRENT;
    }

    private List<SavedTabGroup> getSortedGroupList() {
        List<SavedTabGroup> groupList = new ArrayList<>();
        if (mTabGroupSyncService == null) return groupList;

        for (String syncGroupId : mTabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncGroupId);
            assert !savedTabGroup.savedTabs.isEmpty();

            // To simplify interactions, do not include any groups currently open in other windows.
            if (getState(savedTabGroup) != GroupWindowState.IN_ANOTHER) {
                groupList.add(savedTabGroup);
            }
        }
        groupList.sort((a, b) -> Long.compare(b.creationTimeMs, a.creationTimeMs));
        return groupList;
    }

    private void repopulateModelList() {
        destroyAndClearAllRows();
        LazyOneshotSupplier<CoreAccountInfo> accountInfoSupplier =
                LazyOneshotSupplier.fromSupplier(this::getAccountInfo);

        for (SavedTabGroup savedTabGroup : getSortedGroupList()) {
            TabGroupRowMediator rowMediator =
                    new TabGroupRowMediator(
                            mContext,
                            savedTabGroup,
                            mFilter,
                            mTabGroupSyncService,
                            mDataSharingService,
                            mPaneManager,
                            mTabGroupUiActionHandler,
                            mModalDialogManager,
                            mActionConfirmationManager,
                            mFaviconResolver,
                            accountInfoSupplier,
                            () -> getState(savedTabGroup));
            ListItem listItem = new ListItem(0, rowMediator.getModel());
            mModelList.add(listItem);
        }
        boolean empty = mModelList.isEmpty();
        mPropertyModel.set(TabGroupListProperties.EMPTY_STATE_VISIBLE, empty);
    }

    private void destroyAndClearAllRows() {
        for (ListItem listItem : mModelList) {
            Destroyable destroyable = listItem.model.get(TabGroupRowProperties.DESTROYABLE);
            if (destroyable != null) {
                destroyable.destroy();
            }
        }
        mModelList.clear();
    }

    private CoreAccountInfo getAccountInfo() {
        return mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
    }
}
