// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.BOTTOM_MARGIN_OVERRIDE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.LEFT_MARGIN_OVERRIDE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.RIGHT_MARGIN_OVERRIDE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.DEFAULT_MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupMessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupMessageCardViewProperties.MESSAGING_BACKEND_SERVICE_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.DESTROYABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.bookmarks.PendingRunnable;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
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
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
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
    private final ModalDialogManager mModalDialogManager;
    private final CallbackController mCallbackController = new CallbackController();
    private final @NonNull MessagingBackendService mMessagingBackendService;
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
     * @param modalDialogManager Used to show error dialogs.
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
            ModalDialogManager modalDialogManager) {
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
        mModalDialogManager = modalDialogManager;

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
        destroyAndClearAllRows();
        mFilter.removeObserver(mTabModelObserver);
        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
        }
        mDataSharingService.removeObserver(mDataSharingObserver);
        mSyncService.removeSyncStateChangedListener(mSyncStateChangeListener);
        mCallbackController.destroy();
        mMessagingBackendService.removePersistentMessageObserver(mPersistentMessageObserver);
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

    private List<PropertyModel> getTabGroupRemovedMessageModelList() {
        List<PersistentMessage> messages =
                mMessagingBackendService.getMessages(
                        Optional.of(PersistentNotificationType.TOMBSTONED));

        List<PropertyModel> tabGroupRemovedMessages = new ArrayList<>();
        for (PersistentMessage message : messages) {
            if (message.collaborationEvent != CollaborationEvent.TAB_GROUP_REMOVED) {
                continue;
            }

            if (message.attribution.id == null || TextUtils.isEmpty(message.attribution.id)) {
                continue;
            }

            String descriptionText =
                    mContext.getString(
                            R.string.tab_group_removed_message_card_description,
                            getTabGroupTitle(message));
            String dismissButtonContextDescription =
                    mContext.getString(R.string.accessibility_tab_group_removed_dismiss_button);

            String messageId = message.attribution.id;
            PropertyModel propertyModel =
                    new PropertyModel.Builder(ALL_KEYS)
                            .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                            .with(
                                    UI_DISMISS_ACTION_PROVIDER,
                                    (unused) -> dismissActionProvider(messageId))
                            .with(DESCRIPTION_TEXT, descriptionText)
                            .with(
                                    DISMISS_BUTTON_CONTENT_DESCRIPTION,
                                    dismissButtonContextDescription)
                            .with(IS_ICON_VISIBLE, false)
                            .with(CARD_TYPE, MESSAGE)
                            .with(MESSAGING_BACKEND_SERVICE_ID, messageId)
                            .with(LEFT_MARGIN_OVERRIDE, 0)
                            .with(RIGHT_MARGIN_OVERRIDE, 0)
                            .with(BOTTOM_MARGIN_OVERRIDE, 0)
                            .build();

            tabGroupRemovedMessages.add(propertyModel);
        }
        return tabGroupRemovedMessages;
    }

    private void repopulateModelList() {
        destroyAndClearAllRows();

        for (PropertyModel propertyModel : getTabGroupRemovedMessageModelList()) {
            mModelList.add(new ListItem(RowType.TAB_GROUP_REMOVED_CARD, propertyModel));
        }

        for (SavedTabGroup savedTabGroup : getSortedGroupList()) {
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
                            mModalDialogManager,
                            mActionConfirmationManager,
                            mFaviconResolver,
                            () -> getState(savedTabGroup));
            ListItem listItem = new ListItem(RowType.TAB_GROUP, rowMediator.getModel());
            mModelList.add(listItem);
        }
        boolean empty = mModelList.isEmpty();
        mPropertyModel.set(TabGroupListProperties.EMPTY_STATE_VISIBLE, empty);
    }

    // TODO(crbug.com/394310573): Extract common code to separate util to unify.
    private String getTabGroupTitle(PersistentMessage message) {
        String messageTitle = MessageUtils.extractTabGroupTitle(message);
        if (TextUtils.isEmpty(messageTitle)) {
            @Nullable String syncId = MessageUtils.extractSyncTabGroupId(message);
            @Nullable
            SavedTabGroup syncGroup = syncId == null ? null : mTabGroupSyncService.getGroup(syncId);
            @Nullable Token token = extractLocalId(syncGroup);
            int rootId = mFilter.getRootIdFromStableId(token);
            int tabCount = mFilter.getRelatedTabCountForRootId(rootId);
            return TabGroupTitleUtils.getDisplayableTitle(mContext, mFilter, tabCount);
        } else {
            return messageTitle;
        }
    }

    // TODO(crbug.com/394310573): Extract common code to separate util to unify.
    private @Nullable Token extractLocalId(@Nullable SavedTabGroup syncGroup) {
        return syncGroup == null || syncGroup.localId == null ? null : syncGroup.localId.tabGroupId;
    }

    private void dismissActionProvider(String messageId) {
        removeMessageCardItemFromModelList(messageId);
        mMessagingBackendService.clearPersistentMessage(
                messageId, Optional.of(PersistentNotificationType.TOMBSTONED));
    }

    // TODO(crbug.com/394312504): Make the method general and move to ModelList Util.
    private void removeMessageCardItemFromModelList(String messageId) {
        for (int i = mModelList.size() - 1; i >= 0; i--) {
            ListItem listItem = mModelList.get(i);
            if (listItem.type == RowType.TAB_GROUP_REMOVED_CARD
                    && Objects.equals(
                            listItem.model.get(MESSAGING_BACKEND_SERVICE_ID), messageId)) {
                mModelList.removeAt(i);
                break;
            }
        }
    }

    private void destroyAndClearAllRows() {
        for (ListItem listItem : mModelList) {
            Destroyable destroyable =
                    listItem.model.containsKey(DESTROYABLE)
                            ? listItem.model.get(DESTROYABLE)
                            : null;
            if (destroyable != null) {
                destroyable.destroy();
            }
        }
        mModelList.clear();
    }
}
