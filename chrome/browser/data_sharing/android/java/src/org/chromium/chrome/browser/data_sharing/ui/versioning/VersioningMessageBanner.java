// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import static org.chromium.components.messages.MessageBannerProperties.ALL_KEYS;
import static org.chromium.components.messages.MessageBannerProperties.ICON;
import static org.chromium.components.messages.MessageBannerProperties.MESSAGE_IDENTIFIER;
import static org.chromium.components.messages.MessageBannerProperties.ON_FULLY_VISIBLE;
import static org.chromium.components.messages.MessageBannerProperties.ON_PRIMARY_ACTION;
import static org.chromium.components.messages.MessageBannerProperties.PRIMARY_BUTTON_TEXT;
import static org.chromium.components.messages.MessageBannerProperties.TITLE;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Conditionally constructs and shows a message banner warning the user that they cannot use shared
 * tab groups functionality because of their Chrome. The positive action button will trigger a
 * dialog that will redirect the user to update Chrome.
 */
@NullMarked
public class VersioningMessageBanner {
    /**
     * Conditionally tries to synchronously display UI if the backend tells us to.
     *
     * @param context Used to load resources.
     * @param messageDispatcher Used to show a banner message about out of date version.
     * @param modalDialogManager Used to show a modal dialog about updating version.
     * @param profile Used to fetch scoped dependencies.
     */
    public static void maybeShow(
            Context context,
            MessageDispatcher messageDispatcher,
            ModalDialogManager modalDialogManager,
            Profile profile) {
        // TabGroupSyncService doesn't support OTR. Don't check/prompt the user in this case.
        if (profile.isOffTheRecord()) return;

        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService == null) return;

        VersioningMessageController versioningMessageController =
                tabGroupSyncService.getVersioningMessageController();
        if (versioningMessageController == null) return;

        versioningMessageController.shouldShowMessageUiAsync(
                MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE,
                (Boolean shouldShow) -> {
                    if (!Boolean.TRUE.equals(shouldShow)) return;
                    new VersioningMessageBanner(
                                    context,
                                    messageDispatcher,
                                    modalDialogManager,
                                    versioningMessageController)
                            .show();
                });
    }

    private final Context mContext;
    private final MessageDispatcher mMessageDispatcher;
    private final ModalDialogManager mModalDialogManager;
    private final VersioningMessageController mVersioningMessageController;

    private VersioningMessageBanner(
            Context context,
            MessageDispatcher messageDispatcher,
            ModalDialogManager modalDialogManager,
            VersioningMessageController versioningMessageController) {
        mContext = context;
        mMessageDispatcher = messageDispatcher;
        mModalDialogManager = modalDialogManager;
        mVersioningMessageController = versioningMessageController;
    }

    private void show() {
        mMessageDispatcher.enqueueWindowScopedMessage(buildModel(), /* highPriority= */ false);
    }

    private PropertyModel buildModel() {
        @MessageIdentifier int identifier = MessageIdentifier.UPDATE_CHROME_FOR_TAB_GROUP_SHARE;
        String title =
                mContext.getString(
                        R.string
                                .collaboration_shared_tab_groups_panel_out_of_date_message_cell_text);
        String buttonText = mContext.getString(R.string.update);
        Drawable icon = ContextCompat.getDrawable(mContext, R.drawable.ic_features_24dp);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(MESSAGE_IDENTIFIER, identifier)
                        .with(TITLE, title)
                        .with(PRIMARY_BUTTON_TEXT, buttonText)
                        .with(ICON, icon)
                        .with(ON_PRIMARY_ACTION, this::onPrimary)
                        .with(ON_FULLY_VISIBLE, this::onVisibleChange);
        return builder.build();
    }

    private @PrimaryActionClickBehavior int onPrimary() {
        VersioningModalDialog.show(mContext, mModalDialogManager);
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void onVisibleChange(Boolean fullyVisible) {
        assert fullyVisible != null;
        if (fullyVisible) {
            mVersioningMessageController.onMessageUiShown(
                    MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
        }
    }
}
