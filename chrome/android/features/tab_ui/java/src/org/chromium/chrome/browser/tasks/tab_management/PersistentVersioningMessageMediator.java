// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.ACTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DISMISS_BUTTON_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_ICON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.SHOULD_KEEP_AFTER_REVIEW;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.UI_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.DEFAULT_MESSAGE_IDENTIFIER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.MessageCardType.VERSION_OUT_OF_DATE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType.MESSAGE_CARD;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.ui.versioning.VersioningModalDialog;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.DismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the persistent messaging card for use in {@link TabGroupListCoordinator}.
 * Indicates that the user needs to update their version of Chrome to continue using shared tab
 * groups.
 */
@NullMarked
public class PersistentVersioningMessageMediator {

    /**
     * This is the data type that this mediator uses to create a message card. It holds the data
     * necessary to display the card.
     */
    private static class TabGroupListVersioningMessageData implements MessageData {
        private final Context mContext;
        private final ReviewActionProvider mActionProvider;
        private final DismissActionProvider mDismissActionProvider;

        /**
         * @param context The context used obtaining the message strings.
         * @param actionProvider The provider for the primary action.
         * @param dismissActionProvider The provider for the dismiss action.
         */
        TabGroupListVersioningMessageData(
                Context context,
                ReviewActionProvider actionProvider,
                DismissActionProvider dismissActionProvider) {
            mContext = context;
            mActionProvider = actionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /** The provider for the review action callback. */
        public ReviewActionProvider getActionProvider() {
            return mActionProvider;
        }

        /** The provider for the dismiss action callback. */
        public DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }

        /** Returns the action button text. */
        public String getActionButtonText() {
            return mContext.getString(R.string.update);
        }

        /** Returns the accessibility description for the dismiss button. */
        public String getDismissButtonDescription() {
            // TODO(crbug.com/427950307): Generalize this string's name.
            return mContext.getString(R.string.accessibility_tab_group_removed_dismiss_button);
        }

        /** Returns the formatted message text. */
        public String getMessageText() {
            return mContext.getString(
                    R.string.collaboration_shared_tab_groups_panel_out_of_date_message_cell_text);
        }
    }

    private final Context mContext;
    private final VersioningMessageController mVersioningMessageController;
    private final ModelList mModelList;
    private final ModalDialogManager mModalDialogManager;

    /**
     * @param context The current context.
     * @param versioningMessageController The controller for providing versioning messages.
     * @param modelList The {@link ModelList} to which the message card will be added.
     * @param modalDialogManager The modal dialog manager.
     */
    private PersistentVersioningMessageMediator(
            Context context,
            VersioningMessageController versioningMessageController,
            ModelList modelList,
            ModalDialogManager modalDialogManager) {
        mContext = context;
        mVersioningMessageController = versioningMessageController;
        mModelList = modelList;
        mModalDialogManager = modalDialogManager;
    }

    /** Queues a message card item to be displayed in the model list if required. */
    public void queueMessageIfNeeded() {
        if (!mVersioningMessageController.isInitialized()
                || !mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE)) {
            return;
        }

        removeMessageCard();
        TabGroupListVersioningMessageData messageData =
                new TabGroupListVersioningMessageData(
                        mContext, this::onPrimaryAction, this::onDismiss);

        mModelList.add(new ListItem(MESSAGE_CARD, createPropertyModel(messageData)));

        mVersioningMessageController.onMessageUiShown(
                MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }

    private void onPrimaryAction() {
        VersioningModalDialog.show(mContext, mModalDialogManager);
    }

    private void onDismiss(@MessageService.MessageType int type) {
        removeMessageCard();
        mVersioningMessageController.onMessageUiDismissed(
                MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }

    @VisibleForTesting
    void removeMessageCard() {
        for (int index = 0; index < mModelList.size(); index++) {
            ListItem listItem = mModelList.get(index);
            if (listItem.type != MESSAGE_CARD) return;
            if (isVersioningMessage(listItem.model)) {
                mModelList.removeAt(index);
                break;
            }
        }
    }

    private static boolean isVersioningMessage(PropertyModel model) {
        return model.containsKey(MESSAGE_TYPE) && model.get(MESSAGE_TYPE) == VERSION_OUT_OF_DATE;
    }

    private static PropertyModel createPropertyModel(TabGroupListVersioningMessageData data) {
        String dismissButtonDescription = data.getDismissButtonDescription();

        return new PropertyModel.Builder(TabGroupMessageCardViewProperties.ALL_KEYS)
                .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                .with(ACTION_BUTTON_VISIBLE, true)
                .with(UI_ACTION_PROVIDER, data.getActionProvider())
                .with(ACTION_TEXT, data.getActionButtonText())
                .with(UI_DISMISS_ACTION_PROVIDER, data.getDismissActionProvider())
                .with(SHOULD_KEEP_AFTER_REVIEW, true)
                .with(DESCRIPTION_TEXT, data.getMessageText())
                .with(DISMISS_BUTTON_CONTENT_DESCRIPTION, dismissButtonDescription)
                .with(IS_ICON_VISIBLE, false)
                .with(CARD_TYPE, MESSAGE)
                .with(MESSAGE_TYPE, VERSION_OUT_OF_DATE)
                .with(IS_INCOGNITO, false)
                .build();
    }

    public static @Nullable PersistentVersioningMessageMediator build(
            Context context,
            Profile profile,
            ModelList modelList,
            ModalDialogManager modalDialogManager) {
        assert !profile.isOffTheRecord();

        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService == null) return null;

        VersioningMessageController versioningMessageController =
                tabGroupSyncService.getVersioningMessageController();
        return new PersistentVersioningMessageMediator(
                context, versioningMessageController, modelList, modalDialogManager);
    }
}
