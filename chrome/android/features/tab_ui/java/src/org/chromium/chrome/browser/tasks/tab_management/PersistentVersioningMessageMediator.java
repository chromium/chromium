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
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListCoordinator.RowType.VERSION_OUT_OF_DATE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.ui.versioning.VersioningModalDialog;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ActionProvider;
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
    private static class TabGroupListVersioningMessageModel {
        private final Context mContext;
        private final ActionProvider mAcceptActionProvider;
        private final ActionProvider mDismissActionProvider;

        /**
         * @param context The context used obtaining the message strings.
         * @param acceptActionProvider The provider for the primary action.
         * @param dismissActionProvider The provider for the dismiss action.
         */
        TabGroupListVersioningMessageModel(
                Context context,
                ActionProvider acceptActionProvider,
                ActionProvider dismissActionProvider) {
            mContext = context;
            mAcceptActionProvider = acceptActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /** The provider for the review action callback. */
        public ActionProvider getAcceptActionProvider() {
            return mAcceptActionProvider;
        }

        /** The provider for the dismiss action callback. */
        public ActionProvider getDismissActionProvider() {
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
    private final VersioningModalDialog mVersioningModalDialog;

    /**
     * @param context The current context.
     * @param versioningMessageController The controller for providing versioning messages.
     * @param modelList The {@link ModelList} to which the message card will be added.
     * @param versioningModalDialog The manager for the versioning modal dialog.
     */
    @VisibleForTesting
    PersistentVersioningMessageMediator(
            Context context,
            VersioningMessageController versioningMessageController,
            ModelList modelList,
            VersioningModalDialog versioningModalDialog) {
        mContext = context;
        mVersioningMessageController = versioningMessageController;
        mModelList = modelList;
        mVersioningModalDialog = versioningModalDialog;
    }

    /** Queues a message card item to be displayed in the model list if required. */
    public void queueMessageIfNeeded() {
        if (!mVersioningMessageController.isInitialized()
                || !mVersioningMessageController.shouldShowMessageUi(
                        MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE)) {
            return;
        }

        removeMessageCard();
        TabGroupListVersioningMessageModel messageData =
                new TabGroupListVersioningMessageModel(
                        mContext, this::onPrimaryAction, this::onDismiss);

        mModelList.add(new ListItem(VERSION_OUT_OF_DATE, createPropertyModel(messageData)));

        mVersioningMessageController.onMessageUiShown(
                MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }

    private void onPrimaryAction() {
        mVersioningModalDialog.show();
    }

    private void onDismiss() {
        removeMessageCard();
        mVersioningMessageController.onMessageUiDismissed(
                MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }

    @VisibleForTesting
    void removeMessageCard() {
        for (int index = 0; index < mModelList.size(); index++) {
            ListItem listItem = mModelList.get(index);
            if (listItem.type != VERSION_OUT_OF_DATE) return;
            if (isVersioningMessage(listItem.model)) {
                mModelList.removeAt(index);
                break;
            }
        }
    }

    private static boolean isVersioningMessage(PropertyModel model) {
        return model.containsKey(MESSAGE_TYPE) && model.get(MESSAGE_TYPE) == VERSION_OUT_OF_DATE;
    }

    private static PropertyModel createPropertyModel(TabGroupListVersioningMessageModel data) {
        String dismissButtonDescription = data.getDismissButtonDescription();

        return new PropertyModel.Builder(TabGroupMessageCardViewProperties.ALL_KEYS)
                .with(MESSAGE_IDENTIFIER, DEFAULT_MESSAGE_IDENTIFIER)
                .with(ACTION_BUTTON_VISIBLE, true)
                .with(UI_ACTION_PROVIDER, data.getAcceptActionProvider())
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

    /**
     * Builds a mediator for the persistent messaging card.
     *
     * @param context The context used for obtaining the message strings.
     * @param profile The original profile.
     * @param modelList The model list representing the tab group list.
     * @param modalDialogManager Used to show modal dialogs.
     */
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
        if (versioningMessageController == null) return null;

        VersioningModalDialog versioningModalDialog =
                new VersioningModalDialog(context, modalDialogManager, /* exitRunnable= */ null);

        return new PersistentVersioningMessageMediator(
                context, versioningMessageController, modelList, versioningModalDialog);
    }
}
