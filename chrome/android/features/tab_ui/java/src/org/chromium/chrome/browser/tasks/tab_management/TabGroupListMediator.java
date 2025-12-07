// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DESTROYABLE;
import static org.chromium.ui.modelutil.ModelListCleaner.destroyAndClearAllRows;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;

import org.chromium.base.CallbackController;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubUtils;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Populates a {@link ModelList} with an item for each tab group. */
@NullMarked
public class TabGroupListMediator {
    private final ComponentCallbacks mComponentsCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    setIsTabletOrLandscape();
                }

                @Override
                public void onLowMemory() {}
            };
    private final Context mContext;
    private final ModelList mModelList;
    private final PropertyModel mPropertyModel;
    private final TabGroupModelFilter mFilter;
    private final FaviconResolver mFaviconResolver;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final SyncService mSyncService;
    private final CallbackController mCallbackController = new CallbackController();
    private final MessagingBackendService mMessagingBackendService;
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::repopulateModelList));
    private final boolean mEnableContainment;
    private final DataSharingTabManager mDataSharingTabManager;
    private final TabGroupRemovedMessageMediator mTabGroupRemovedMessageMediator;
    private final @Nullable PersistentVersioningMessageMediator
            mPersistentVersioningMessageMediator;

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

    private final Observer mTabGroupSyncObserver =
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
                    if (!enabled) {
                        // When sign out happens, we need to clear the message cards. There is no
                        // other signal that will do this, hence we explicitly clear and rebuild the
                        // list.
                        // TODO(crbug.com/398901000): Build this into backend service observer.
                        repopulateModelList();
                    }
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

    private final PersistentMessageObserver mPersistentMessageObserver =
            new PersistentMessageObserver() {
                @Override
                public void onMessagingBackendServiceInitialized() {
                    mPendingRefresh.post();
                }

                @Override
                public void displayPersistentMessage(PersistentMessage message) {
                    if (message.collaborationEvent == CollaborationEvent.TAB_GROUP_REMOVED) {
                        mPendingRefresh.post();
                    }
                }

                @Override
                public void hidePersistentMessage(PersistentMessage message) {
                    if (message.collaborationEvent == CollaborationEvent.TAB_GROUP_REMOVED) {
                        mPendingRefresh.post();
                    }
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
     * @param collaborationService Used to fetch collaboration group data.
     * @param messagingBackendService Used to fetch tab group related messages.
     * @param paneManager Used switch panes to show details of a group.
     * @param tabGroupUiActionHandler Used to open hidden tab groups.
     * @param actionConfirmationManager Used to show confirmation dialogs.
     * @param syncService Used to query active sync types.
     * @param enableContainment Whether containment is enabled.
     * @param dataSharingTabManager The {@link} DataSharingTabManager to start collaboration flows.
     * @param tabGroupRemovedMessageMediator The mediator for the tab group removed message card.
     * @param persistentVersioningMessageMediator Used to show persistent versioning messages.
     */
    public TabGroupListMediator(
            Context context,
            ModelList modelList,
            PropertyModel propertyModel,
            TabGroupModelFilter filter,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            DataSharingService dataSharingService,
            CollaborationService collaborationService,
            MessagingBackendService messagingBackendService,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ActionConfirmationManager actionConfirmationManager,
            SyncService syncService,
            boolean enableContainment,
            DataSharingTabManager dataSharingTabManager,
            TabGroupRemovedMessageMediator tabGroupRemovedMessageMediator,
            @Nullable PersistentVersioningMessageMediator persistentVersioningMessageMediator) {
        mContext = context;
        mModelList = modelList;
        mPropertyModel = propertyModel;
        mFilter = filter;
        mFaviconResolver = faviconResolver;
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;
        mMessagingBackendService = messagingBackendService;
        mPaneManager = paneManager;
        mTabGroupUiActionHandler = tabGroupUiActionHandler;
        mActionConfirmationManager = actionConfirmationManager;
        mSyncService = syncService;
        mEnableContainment = enableContainment;
        mDataSharingTabManager = dataSharingTabManager;
        mTabGroupRemovedMessageMediator = tabGroupRemovedMessageMediator;
        mPersistentVersioningMessageMediator = persistentVersioningMessageMediator;

        mFilter.addObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }
        mDataSharingService.addObserver(mDataSharingObserver);
        mSyncService.addSyncStateChangedListener(mSyncStateChangeListener);
        mMessagingBackendService.addPersistentMessageObserver(mPersistentMessageObserver);
        mContext.registerComponentCallbacks(mComponentsCallbacks);

        repopulateModelList();
        mSyncStateChangeListener.syncStateChanged();
    }

    /** Clean up observers used by this class. */
    public void destroy() {
        destroyAndClearAllRows(mModelList, DESTROYABLE);
        mFilter.removeObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
        }
        mDataSharingService.removeObserver(mDataSharingObserver);
        mSyncService.removeSyncStateChangedListener(mSyncStateChangeListener);
        mCallbackController.destroy();
        mMessagingBackendService.removePersistentMessageObserver(mPersistentMessageObserver);
        mContext.unregisterComponentCallbacks(mComponentsCallbacks);
    }

    private void repopulateModelList() {
        destroyAndClearAllRows(mModelList, DESTROYABLE);
        mTabGroupRemovedMessageMediator.queueMessageIfNeeded();
        if (mPersistentVersioningMessageMediator != null) {
            mPersistentVersioningMessageMediator.queueMessageIfNeeded();
        }

        GroupWindowChecker sortUtil = new GroupWindowChecker(mTabGroupSyncService, mFilter);
        List<SavedTabGroup> sortedTabGroups =
                sortUtil.getSortedGroupList(
                        this::shouldShowGroupByState,
                        (a, b) -> {
                            if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups
                                    .isEnabled()) {
                                return Long.compare(
                                        TabUiUtils.getGroupLastUpdatedTimestamp(b),
                                        TabUiUtils.getGroupLastUpdatedTimestamp(a));
                            } else {
                                return Long.compare(b.creationTimeMs, a.creationTimeMs);
                            }
                        });
        for (SavedTabGroup savedTabGroup : sortedTabGroups) {
            TabGroupRowMediator rowMediator =
                    new TabGroupRowMediator(
                            mContext,
                            savedTabGroup,
                            mFilter,
                            assumeNonNull(mTabGroupSyncService),
                            mDataSharingService,
                            mCollaborationService,
                            mPaneManager,
                            mTabGroupUiActionHandler,
                            mActionConfirmationManager,
                            mFaviconResolver,
                            () -> sortUtil.getState(savedTabGroup),
                            mEnableContainment,
                            mDataSharingTabManager);
            ListItem listItem = new ListItem(RowType.TAB_GROUP, rowMediator.getModel());
            mModelList.add(listItem);
        }
        boolean empty = mModelList.isEmpty();
        mPropertyModel.set(TabGroupListProperties.EMPTY_STATE_VISIBLE, empty);

        setIsTabletOrLandscape();
    }

    private void setIsTabletOrLandscape() {
        if (OmniboxFeatures.sAndroidHubSearchTabGroups.isEnabled()
                && OmniboxFeatures.sAndroidHubSearchEnableOnTabGroupsPane.getValue()) {
            Configuration config = mContext.getResources().getConfiguration();
            boolean isTabletOrLandscape = HubUtils.isScreenWidthTablet(config.screenWidthDp);
            mPropertyModel.set(TabGroupListProperties.IS_TABLET_OR_LANDSCAPE, isTabletOrLandscape);
        } else {
            // No search box to make space for.
            mPropertyModel.set(TabGroupListProperties.IS_TABLET_OR_LANDSCAPE, true);
        }
    }

    private boolean shouldShowGroupByState(@GroupWindowState int groupWindowState) {
        return groupWindowState != GroupWindowState.IN_ANOTHER;
    }
}
