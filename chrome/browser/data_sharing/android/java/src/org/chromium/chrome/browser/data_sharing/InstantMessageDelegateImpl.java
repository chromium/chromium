// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.InstantMessage;
import org.chromium.components.collaboration.messaging.InstantNotificationLevel;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.InstantMessageDelegate;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Responsible for displaying browser and OS messages for share. This is effectively a singleton,
 * scoped by profile. This class should be attached/detached by all windows.
 */
public class InstantMessageDelegateImpl implements InstantMessageDelegate {
    private static class AttachedWindowInfo {
        public final WindowAndroid windowAndroid;
        public final TabGroupModelFilter tabGroupModelFilter;
        public final DataSharingNotificationManager dataSharingNotificationManager;

        public AttachedWindowInfo(
                WindowAndroid windowAndroid,
                TabGroupModelFilter tabGroupModelFilter,
                DataSharingNotificationManager dataSharingNotificationManager) {
            this.windowAndroid = windowAndroid;
            this.tabGroupModelFilter = tabGroupModelFilter;
            this.dataSharingNotificationManager = dataSharingNotificationManager;
        }
    }

    private final List<AttachedWindowInfo> mAttachList = new ArrayList<>();

    /**
     * @param profile The current profile to get dependencies with.
     */
    /* package */ InstantMessageDelegateImpl(Profile profile) {
        profile = profile.getOriginalProfile();
        MessagingBackendService messagingBackendService =
                MessagingBackendServiceFactory.getForProfile(profile);
        messagingBackendService.setInstantMessageDelegate(this);
    }

    /**
     * @param windowAndroid The window that can be used for showing messages.
     * @param tabGroupModelFilter The tab model and group filter for the given window.
     * @param dataSharingNotificationManager Used to send notifications for a particular window.
     */
    public void attachWindow(
            @NonNull WindowAndroid windowAndroid,
            @NonNull TabGroupModelFilter tabGroupModelFilter,
            @NonNull DataSharingNotificationManager dataSharingNotificationManager) {
        assert windowAndroid != null;
        assert tabGroupModelFilter != null;
        assert !tabGroupModelFilter.isIncognito();
        assert dataSharingNotificationManager != null;
        mAttachList.add(
                new AttachedWindowInfo(
                        windowAndroid, tabGroupModelFilter, dataSharingNotificationManager));
    }

    /**
     * @param windowAndroid The window that is no longer usable for showing messages.
     */
    public void detachWindow(@NonNull WindowAndroid windowAndroid) {
        assert windowAndroid != null;
        mAttachList.removeIf(awi -> Objects.equals(awi.windowAndroid, windowAndroid));
    }

    @Override
    public void displayInstantaneousMessage(
            InstantMessage message, Callback<Boolean> successCallback) {
        @Nullable AttachedWindowInfo attachedWindowInfo = getAttachedWindowInfo(message);
        if (attachedWindowInfo == null) {
            successCallback.onResult(false);
            return;
        }

        @NonNull WindowAndroid windowAndroid = attachedWindowInfo.windowAndroid;
        @Nullable Context context = windowAndroid.getContext().get();
        if (context == null) {
            successCallback.onResult(false);
            return;
        }

        @NonNull TabGroupModelFilter tabGroupModelFilter = attachedWindowInfo.tabGroupModelFilter;
        @CollaborationEvent int collaborationEvent = message.collaborationEvent;

        if (message.level == InstantNotificationLevel.SYSTEM) {
            if (collaborationEvent == CollaborationEvent.COLLABORATION_MEMBER_ADDED) {
                @NonNull
                DataSharingNotificationManager dataSharingNotificationManager =
                        attachedWindowInfo.dataSharingNotificationManager;
                showCollaborationMemberAddedSystemNotification(
                        message, context, dataSharingNotificationManager, tabGroupModelFilter);
            }
            successCallback.onResult(true);
        } else if (message.level == InstantNotificationLevel.BROWSER) {
            @Nullable
            MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
            if (messageDispatcher == null) {
                successCallback.onResult(false);
                return;
            }

            Runnable onSuccess = successCallback.bind(true);
            if (collaborationEvent == CollaborationEvent.TAB_REMOVED) {
                showTabRemoved(message, context, messageDispatcher, onSuccess);
            } else if (collaborationEvent == CollaborationEvent.TAB_UPDATED) {
                showTabChange(message, context, messageDispatcher, onSuccess);
            } else if (collaborationEvent == CollaborationEvent.COLLABORATION_MEMBER_ADDED) {
                showCollaborationMemberAdded(
                        message, context, messageDispatcher, tabGroupModelFilter, onSuccess);
            } else if (collaborationEvent == CollaborationEvent.COLLABORATION_REMOVED) {
                showCollaborationRemoved(
                        message, context, messageDispatcher, tabGroupModelFilter, onSuccess);
            } else {
                // Will never be able to handle this message.
                onSuccess.run();
            }
        }
    }

    private AttachedWindowInfo getAttachedWindowInfo(InstantMessage message) {
        if (mAttachList.size() == 0) {
            return null;
        }

        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        if (tabGroupId == null) {
            // Message doesn't link to a window, show it arbitrarily.
            return mAttachList.get(0);
        }

        for (AttachedWindowInfo info : mAttachList) {
            TabGroupModelFilter tabGroupModelFilter = info.tabGroupModelFilter;
            int rootId = tabGroupModelFilter.getRootIdFromStableId(tabGroupId);
            if (rootId == Tab.INVALID_TAB_ID) continue;

            // If we had a valid rootId, this is the right window.
            return info;
        }

        // Tab group was deleted or window not active.
        return null;
    }

    private Drawable iconFromMessage(Context context) {
        // TODO(https://crbug.com/369163940): Fetch this, potentially async.
        return ContextCompat.getDrawable(context, R.drawable.ic_features_24dp);
    }

    private void showTabRemoved(
            InstantMessage message,
            Context context,
            MessageDispatcher messageDispatcher,
            Runnable onSuccess) {
        String givenName = MessageUtils.extractGivenName(message);
        String tabTitle = MessageUtils.extractTabTitle(message);
        String title =
                context.getString(
                        R.string.data_sharing_browser_message_removed_tab, givenName, tabTitle);
        String buttonText = context.getString(R.string.data_sharing_browser_message_reopen);
        Drawable icon = iconFromMessage(context);
        // TODO(https://crbug.com/369163940): Once the message has the url, we can restore.
        showGenericMessage(
                messageDispatcher,
                MessageIdentifier.TAB_REMOVED_THROUGH_COLLABORATION,
                title,
                buttonText,
                icon,
                () -> {},
                onSuccess);
    }

    private void showTabChange(
            InstantMessage message,
            Context context,
            MessageDispatcher messageDispatcher,
            Runnable onSuccess) {
        String givenName = MessageUtils.extractGivenName(message);
        String tabTitle = MessageUtils.extractTabTitle(message);
        String title =
                context.getString(
                        R.string.data_sharing_browser_message_changed_tab, givenName, tabTitle);
        String buttonText = context.getString(R.string.data_sharing_browser_message_reopen);
        Drawable icon = iconFromMessage(context);
        // TODO(https://crbug.com/369163940): Once the message has the url, we can restore.
        showGenericMessage(
                messageDispatcher,
                MessageIdentifier.TAB_NAVIGATED_THROUGH_COLLABORATION,
                title,
                buttonText,
                icon,
                () -> {},
                onSuccess);
    }

    private void showCollaborationMemberAdded(
            InstantMessage message,
            Context context,
            MessageDispatcher messageDispatcher,
            TabGroupModelFilter tabGroupModelFilter,
            Runnable onSuccess) {
        String givenName = MessageUtils.extractGivenName(message);
        String tabGroupTitle = getTabGroupTitle(message, context, tabGroupModelFilter);
        String title =
                context.getString(
                        R.string.data_sharing_browser_message_joined_tab_group,
                        givenName,
                        tabGroupTitle);
        String buttonText = context.getString(R.string.data_sharing_browser_message_manage);
        Drawable icon = iconFromMessage(context);
        // TODO(https://crbug.com/369163940): Action should open manage sheet.
        showGenericMessage(
                messageDispatcher,
                MessageIdentifier.COLLABORATION_MEMBER_ADDED,
                title,
                buttonText,
                icon,
                () -> {},
                onSuccess);
    }

    private void showCollaborationRemoved(
            InstantMessage message,
            Context context,
            MessageDispatcher messageDispatcher,
            TabGroupModelFilter tabGroupModelFilter,
            Runnable onSuccess) {
        String tabGroupTitle = getTabGroupTitle(message, context, tabGroupModelFilter);
        String title =
                context.getString(
                        R.string.data_sharing_browser_message_not_available, tabGroupTitle);
        String buttonText = context.getString(R.string.data_sharing_invitation_failure_button);
        Drawable icon = ContextCompat.getDrawable(context, R.drawable.ic_features_24dp);
        showGenericMessage(
                messageDispatcher,
                MessageIdentifier.COLLABORATION_REMOVED,
                title,
                buttonText,
                icon,
                () -> {},
                onSuccess);
    }

    private void showCollaborationMemberAddedSystemNotification(
            InstantMessage message,
            Context context,
            DataSharingNotificationManager dataSharingNotificationManager,
            TabGroupModelFilter tabGroupModelFilter) {
        String givenName = MessageUtils.extractGivenName(message);
        String tabGroupTitle = getTabGroupTitle(message, context, tabGroupModelFilter);
        String contentTitle =
                context.getString(
                        R.string.data_sharing_browser_message_joined_tab_group,
                        givenName,
                        tabGroupTitle);

        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        dataSharingNotificationManager.showOtherJoinedNotification(contentTitle, tabGroupId);
    }

    private void showGenericMessage(
            MessageDispatcher messageDispatcher,
            @MessageIdentifier int messageIdentifier,
            String title,
            String buttonText,
            Drawable icon,
            Runnable action,
            Runnable onSuccess) {
        Supplier<Integer> onPrimary =
                () -> {
                    action.run();
                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                };
        Callback<Boolean> onVisibleChange =
                (fullyVisible) -> {
                    if (fullyVisible) {
                        onSuccess.run();
                    }
                };
        PropertyModel propertyModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER, messageIdentifier)
                        .with(MessageBannerProperties.TITLE, title)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, buttonText)
                        .with(MessageBannerProperties.ICON, icon)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, onPrimary)
                        .with(MessageBannerProperties.ON_FULLY_VISIBLE, onVisibleChange)
                        .build();
        messageDispatcher.enqueueWindowScopedMessage(propertyModel, /* highPriority= */ false);
    }

    private String getTabGroupTitle(
            InstantMessage message, Context context, TabGroupModelFilter tabGroupModelFilter) {
        String messageTitle = MessageUtils.extractTabGroupTitle(message);
        if (TextUtils.isEmpty(messageTitle)) {
            // Shouldn't need to check for any failure cases, should claim 1 tab if not found.
            Token token = MessageUtils.extractTabGroupId(message);
            int rootId = tabGroupModelFilter.getRootIdFromStableId(token);
            int tabCount = tabGroupModelFilter.getRelatedTabCountForRootId(rootId);
            return TabGroupTitleUtils.getDefaultTitle(context, tabCount);
        } else {
            return messageTitle;
        }
    }
}
