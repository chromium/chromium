// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.BOTTOM_MARGIN_OVERRIDE_PX;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.LEFT_MARGIN_OVERRIDE_PX;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.RIGHT_MARGIN_OVERRIDE_PX;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.DEFAULT_MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupMessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DESTROYABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.ui.modelutil.ModelListCleaner.destroyAndClearAllRows;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
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

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Populates a {@link ModelList} with an item for each tab group. */
public class TabGroupListMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final PropertyModel mPropertyModel;
    private final TabGroupModelFilter mFilter;
    private final FaviconResolver mFaviconResolver;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final @NonNull DataSharingService mDataSharingService;
    private final @NonNull CollaborationService mCollaborationService;
    private final PaneManager mPaneManager;
    private final TabGroupUiActionHandler mTabGroupUiActionHandler;
    private final ActionConfirmationManager mActionConfirmationManager;
    private final SyncService mSyncService;
    private final CallbackController mCallbackController = new CallbackController();
    private final @NonNull MessagingBackendService mMessagingBackendService;
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT,
                    mCallbackController.makeCancelable(this::repopulateModelList));
    private final boolean mEnableContainment;
    private final DataSharingTabManager mDataSharingTabManager;

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
     */
    public TabGroupListMediator(
            Context context,
            ModelList modelList,
            PropertyModel propertyModel,
            TabGroupModelFilter filter,
            FaviconResolver faviconResolver,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @NonNull DataSharingService dataSharingService,
            @NonNull CollaborationService collaborationService,
            @NonNull MessagingBackendService messagingBackendService,
            PaneManager paneManager,
            TabGroupUiActionHandler tabGroupUiActionHandler,
            ActionConfirmationManager actionConfirmationManager,
            SyncService syncService,
            boolean enableContainment,
            @NonNull DataSharingTabManager dataSharingTabManager) {
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

        mFilter.addObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }
        mDataSharingService.addObserver(mDataSharingObserver);
        mSyncService.addSyncStateChangedListener(mSyncStateChangeListener);
        mMessagingBackendService.addPersistentMessageObserver(mPersistentMessageObserver);

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
    }

    private void repopulateModelList() {
        destroyAndClearAllRows(mModelList, DESTROYABLE);

        List<PersistentMessage> tabGroupRemovedMessages = getTabGroupRemovedMessageList();
        if (!tabGroupRemovedMessages.isEmpty()) {
            mModelList.add(
                    new ListItem(
                            RowType.TAB_GROUP_REMOVED_CARD,
                            buildTabGroupMessageModel(tabGroupRemovedMessages)));
        }

        GroupWindowChecker sortUtil = new GroupWindowChecker(mTabGroupSyncService, mFilter);
        List<SavedTabGroup> sortedTabGroups =
                sortUtil.getSortedGroupList(
                        this::shouldShowGroupByState,
                        (a, b) -> Long.compare(b.creationTimeMs, a.creationTimeMs));
        for (SavedTabGroup savedTabGroup : sortedTabGroups) {
            TabGroupRowMediator rowMediator =
                    new TabGroupRowMediator(
                            mContext,
                            savedTabGroup,
                            mFilter,
                            mTabGroupSyncService,
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
    }

    private List<PersistentMessage> getTabGroupRemovedMessageList() {
        List<PersistentMessage> tabGroupRemovedMessages = new ArrayList<>();
        List<PersistentMessage> messages =
                mMessagingBackendService.getMessages(
                        Optional.of(PersistentNotificationType.TOMBSTONED));

        for (PersistentMessage message : messages) {
            if (message.collaborationEvent != CollaborationEvent.TAB_GROUP_REMOVED
                    || !TabShareUtils.isCollaborationIdValid(message.attribution.id)) {
                continue;
            }

            tabGroupRemovedMessages.add(message);
        }
        return tabGroupRemovedMessages;
    }

    private PropertyModel buildTabGroupMessageModel(
            List<PersistentMessage> tabGroupRemovedMessages) {
        assert !tabGroupRemovedMessages.isEmpty();
        String dismissButtonContextDescription =
                mContext.getString(R.string.accessibility_tab_group_removed_dismiss_button);
        int horizontalPadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_group_removed_card_horizontal_padding);

        return new PropertyModel.Builder(ALL_KEYS)
                .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                .with(
                        UI_DISMISS_ACTION_PROVIDER,
                        (unused) -> dismissActionProvider(tabGroupRemovedMessages))
                .with(
                        DESCRIPTION_TEXT,
                        getTabGroupMessageRemovedDescriptionText(tabGroupRemovedMessages))
                .with(DISMISS_BUTTON_CONTENT_DESCRIPTION, dismissButtonContextDescription)
                .with(IS_ICON_VISIBLE, false)
                .with(CARD_TYPE, MESSAGE)
                .with(ACTION_BUTTON_VISIBLE, false)
                .with(LEFT_MARGIN_OVERRIDE_PX, horizontalPadding)
                .with(RIGHT_MARGIN_OVERRIDE_PX, horizontalPadding)
                .with(BOTTOM_MARGIN_OVERRIDE_PX, 0)
                .build();
    }

    private String getTabGroupMessageRemovedDescriptionText(
            List<PersistentMessage> tabGroupRemovedMessages) {
        List<String> messageTitles = new ArrayList<>();
        int removedGroupsCount = tabGroupRemovedMessages.size();
        for (PersistentMessage message : tabGroupRemovedMessages) {
            messageTitles.add(MessageUtils.extractTabGroupTitle(message));
        }

        // If title is present.
        if (removedGroupsCount == 1 && !TextUtils.isEmpty(messageTitles.get(0))) {
            return mContext.getString(
                    R.string.one_tab_group_removed_message_card_description, messageTitles.get(0));
        }
        // If both titles are present.
        else if (removedGroupsCount == 2
                && !TextUtils.isEmpty(messageTitles.get(0))
                && !TextUtils.isEmpty(messageTitles.get(1))) {
            return mContext.getString(
                    R.string.two_tab_groups_removed_message_card_description,
                    messageTitles.get(0),
                    messageTitles.get(1));
        } else {
            // When either titles are not present OR count is more than 2.
            return mContext.getResources()
                    .getQuantityString(
                            R.plurals.generic_tab_groups_removed_message_card_description,
                            removedGroupsCount,
                            removedGroupsCount);
        }
    }

    private void dismissActionProvider(List<PersistentMessage> tabGroupRemovedMessages) {
        for (PersistentMessage message : tabGroupRemovedMessages) {
            // Since we are only storing messages with non-empty ID.
            @Nullable String messageId = message.attribution.id;
            assert messageId != null && !TextUtils.isEmpty(messageId);
            mMessagingBackendService.clearPersistentMessage(
                    messageId, Optional.of(PersistentNotificationType.TOMBSTONED));
        }
        removeMessageCardItemFromModelList();
    }

    private void removeMessageCardItemFromModelList() {
        if (mModelList.isEmpty()) return;
        if (mModelList.get(0).type != RowType.TAB_GROUP_REMOVED_CARD) return;

        // There can only one message card.
        mModelList.removeAt(0);
        assert mModelList.isEmpty() || mModelList.get(0).type != RowType.TAB_GROUP_REMOVED_CARD;
    }

    private boolean shouldShowGroupByState(@GroupWindowState int groupWindowState) {
        return groupWindowState != GroupWindowState.IN_ANOTHER;
    }
}
