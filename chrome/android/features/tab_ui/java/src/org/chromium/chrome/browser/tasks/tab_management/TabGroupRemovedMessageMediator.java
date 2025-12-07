// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.BOTTOM_MARGIN_OVERRIDE_PX;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.LEFT_MARGIN_OVERRIDE_PX;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.RIGHT_MARGIN_OVERRIDE_PX;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.DEFAULT_MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType.TAB_GROUP_REMOVED;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ActionProvider;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** A mediator for the tab group removed message card for use in {@link TabGroupListCoordinator}. */
@NullMarked
public class TabGroupRemovedMessageMediator {

    /**
     * This is the data type that this mediator uses to create a message card. It holds the data
     * necessary to display the card.
     */
    private static class TabGroupRemovedMessageModel {
        private final Context mContext;
        private final List<PersistentMessage> mTabGroupRemovedMessages;
        private final ActionProvider mDismissActionProvider;

        /**
         * @param context The context used obtaining the message strings.
         * @param tabGroupRemovedMessages The list of persistent messages.
         * @param dismissActionProvider The provider for the dismiss action.
         */
        TabGroupRemovedMessageModel(
                Context context,
                List<PersistentMessage> tabGroupRemovedMessages,
                ActionProvider dismissActionProvider) {
            mContext = context;
            mTabGroupRemovedMessages = tabGroupRemovedMessages;
            mDismissActionProvider = dismissActionProvider;
        }

        /** The provider for the dismiss action callback. */
        public ActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }

        /** Returns the accessibility description for the dismiss button. */
        public String getDismissButtonDescription() {
            return mContext.getString(R.string.accessibility_tab_group_removed_dismiss_button);
        }

        /** Returns the horizontal padding for the message card in pixels. */
        public int getHorizontalPadding() {
            return mContext.getResources()
                    .getDimensionPixelSize(R.dimen.tab_group_removed_card_horizontal_padding);
        }

        /** Returns the formatted message text. */
        public String getMessageText() {
            List<String> messageTitles = new ArrayList<>();
            int removedGroupsCount = mTabGroupRemovedMessages.size();
            for (PersistentMessage message : mTabGroupRemovedMessages) {
                messageTitles.add(MessageUtils.extractTabGroupTitle(message));
            }

            // If title is present.
            if (removedGroupsCount == 1 && !TextUtils.isEmpty(messageTitles.get(0))) {
                return mContext.getString(
                        R.string.one_tab_group_removed_message_card_description,
                        messageTitles.get(0));
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
    }

    private final Context mContext;
    private final MessagingBackendService mMessagingBackendService;
    private final ModelList mModelList;

    /**
     * @param context The current context.
     * @param messagingBackendService The service for fetching persistent messages.
     * @param modelList The {@link ModelList} to which the message card will be added.
     */
    public TabGroupRemovedMessageMediator(
            Context context, MessagingBackendService messagingBackendService, ModelList modelList) {
        mContext = context;
        mMessagingBackendService = messagingBackendService;
        mModelList = modelList;
    }

    /**
     * Fetches tab group removed messages and, if any are found, creates and queues a message card
     * item to be displayed in the model list.
     */
    public void queueMessageIfNeeded() {
        List<PersistentMessage> messageList = getTabGroupRemovedMessageList();
        if (messageList.isEmpty()) return;

        TabGroupRemovedMessageModel messageData =
                new TabGroupRemovedMessageModel(
                        mContext, messageList, () -> dismissAction(messageList));

        mModelList.add(
                new MVCListAdapter.ListItem(TAB_GROUP_REMOVED, createPropertyModel(messageData)));
    }

    private void dismissAction(List<PersistentMessage> tabGroupRemovedMessages) {
        for (PersistentMessage message : tabGroupRemovedMessages) {
            // Since we are only storing messages with non-empty ID.
            @Nullable String messageId = message.attribution.id;
            assert messageId != null && !TextUtils.isEmpty(messageId);
            mMessagingBackendService.clearPersistentMessage(
                    messageId, PersistentNotificationType.TOMBSTONED);
        }
        removeMessageCard();
    }

    @VisibleForTesting
    void removeMessageCard() {
        for (int index = 0; index < mModelList.size(); index++) {
            MVCListAdapter.ListItem listItem = mModelList.get(index);
            if (listItem.type != TAB_GROUP_REMOVED) return;
            if (isGroupRemovedMessage(listItem.model)) {
                mModelList.removeAt(index);
                break;
            }
        }
    }

    private static boolean isGroupRemovedMessage(PropertyModel model) {
        return model.containsKey(MESSAGE_TYPE) && model.get(MESSAGE_TYPE) == TAB_GROUP_REMOVED;
    }

    private List<PersistentMessage> getTabGroupRemovedMessageList() {
        List<PersistentMessage> tabGroupRemovedMessages = new ArrayList<>();
        List<PersistentMessage> messages =
                mMessagingBackendService.getMessages(PersistentNotificationType.TOMBSTONED);

        for (PersistentMessage message : messages) {
            if (message.collaborationEvent != CollaborationEvent.TAB_GROUP_REMOVED
                    || !TabShareUtils.isCollaborationIdValid(message.attribution.id)) {
                continue;
            }

            tabGroupRemovedMessages.add(message);
        }
        return tabGroupRemovedMessages;
    }

    private static PropertyModel createPropertyModel(TabGroupRemovedMessageModel data) {
        String dismissButtonDescription = data.getDismissButtonDescription();
        int horizontalPadding = data.getHorizontalPadding();

        return new PropertyModel.Builder(TabGroupMessageCardViewProperties.ALL_KEYS)
                .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                .with(UI_DISMISS_ACTION_PROVIDER, data.getDismissActionProvider())
                .with(DESCRIPTION_TEXT, data.getMessageText())
                .with(DISMISS_BUTTON_CONTENT_DESCRIPTION, dismissButtonDescription)
                .with(IS_ICON_VISIBLE, false)
                .with(CARD_TYPE, MESSAGE)
                .with(MESSAGE_TYPE, TAB_GROUP_REMOVED)
                .with(ACTION_BUTTON_VISIBLE, false)
                .with(LEFT_MARGIN_OVERRIDE_PX, horizontalPadding)
                .with(RIGHT_MARGIN_OVERRIDE_PX, horizontalPadding)
                .with(BOTTOM_MARGIN_OVERRIDE_PX, 0)
                .with(IS_INCOGNITO, false)
                .build();
    }
}
