// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.tab_group_sync.messaging.InstantMessage;
import org.chromium.components.tab_group_sync.messaging.InstantNotificationLevel;
import org.chromium.components.tab_group_sync.messaging.MessageAttribution;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService.InstantMessageDelegate;
import org.chromium.components.tab_group_sync.messaging.UserAction;
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
    private final List<Pair<WindowAndroid, TabGroupModelFilter>> mAttachList = new ArrayList<>();

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
     */
    public void attachWindow(
            @NonNull WindowAndroid windowAndroid,
            @NonNull TabGroupModelFilter tabGroupModelFilter) {
        assert windowAndroid != null;
        assert tabGroupModelFilter != null;
        assert !tabGroupModelFilter.isIncognito();
        mAttachList.add(new Pair<>(windowAndroid, tabGroupModelFilter));
    }

    /**
     * @param windowAndroid The window that is no longer usable for showing messages.
     */
    public void detachWindow(@NonNull WindowAndroid windowAndroid) {
        assert windowAndroid != null;
        mAttachList.removeIf(wa -> Objects.equals(wa.first, windowAndroid));
    }

    @Override
    public void displayInstantaneousMessage(
            InstantMessage message, Callback<Boolean> successCallback) {
        boolean success = false;
        try {
            if (message.level == InstantNotificationLevel.SYSTEM) {
                // TODO(https://crbug.com/369164214): Implement.
            } else if (message.level == InstantNotificationLevel.BROWSER) {
                @Nullable
                Pair<WindowAndroid, TabGroupModelFilter> attach = getAttach(message.attribution);
                if (attach == null) return;
                @NonNull WindowAndroid windowAndroid = attach.first;
                @NonNull TabGroupModelFilter tabGroupModelFilter = attach.second;
                @Nullable
                MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
                @Nullable Context context = windowAndroid.getContext().get();
                if (messageDispatcher == null || context == null) return;

                @UserAction int userAction = message.action;
                if (userAction == UserAction.TAB_REMOVED) {
                    showTabRemoved(message, context, messageDispatcher);
                } else if (userAction == UserAction.TAB_NAVIGATED) {
                    showTabChange(message, context, messageDispatcher);
                } else if (userAction == UserAction.COLLABORATION_USER_JOINED) {
                    showCollaborationUserJoined(message, context, messageDispatcher);
                } else if (userAction == UserAction.COLLABORATION_REMOVED) {
                    showCollaborationRemoved(message, context, messageDispatcher);
                }
                success = true;
            }
        } finally {
            // TODO(https://crbug.com/369164214): Implement correct usage of `successCallback`.
            successCallback.onResult(success);
        }
    }

    private Pair<WindowAndroid, TabGroupModelFilter> getAttach(
            MessageAttribution messageAttribution) {
        if (mAttachList.size() == 0) {
            return null;
        }

        if (messageAttribution == null
                || messageAttribution.localTabGroupId == null
                || messageAttribution.localTabGroupId.tabGroupId == null) {
            // Message doesn't link to a window, show it arbitrarily.
            return mAttachList.get(0);
        }

        @NonNull Token tabGroupId = messageAttribution.localTabGroupId.tabGroupId;
        for (Pair<WindowAndroid, TabGroupModelFilter> attach : mAttachList) {
            TabGroupModelFilter tabGroupModelFilter = attach.second;
            int rootId = tabGroupModelFilter.getRootIdFromStableId(tabGroupId);
            if (rootId == Tab.INVALID_TAB_ID) continue;

            // If we had a valid rootId, this is the right window.
            return attach;
        }

        // Tab group was deleted or window not active.
        return null;
    }

    private String givenNameFromMessage(InstantMessage message) {
        return message.attribution.triggeringUser.givenName;
    }

    private String tabTitleFromMessage() {
        // TODO(https://crbug.com/369163940): Once the message stores this, we can return it.
        return "ph1";
    }

    private String tabGroupTitleFromMessage() {
        // TODO(https://crbug.com/369163940): Once the message stores this, we can return it.
        return "ph2";
    }

    private Drawable iconFromMessage(Context context) {
        // TODO(https://crbug.com/369163940): Fetch this, potentially async.
        return ContextCompat.getDrawable(context, R.drawable.ic_features_24dp);
    }

    private void showTabRemoved(
            InstantMessage message, Context context, MessageDispatcher messageDispatcher) {
        String givenName = givenNameFromMessage(message);
        String tabTitle = tabTitleFromMessage();
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
                () -> {});
    }

    private void showTabChange(
            InstantMessage message, Context context, MessageDispatcher messageDispatcher) {
        String givenName = givenNameFromMessage(message);
        String tabTitle = tabTitleFromMessage();
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
                () -> {});
    }

    private void showCollaborationUserJoined(
            InstantMessage message, Context context, MessageDispatcher messageDispatcher) {
        String givenName = givenNameFromMessage(message);
        String tabGroupTitle = tabGroupTitleFromMessage();
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
                MessageIdentifier.COLLABORATION_USER_JOINED,
                title,
                buttonText,
                icon,
                () -> {});
    }

    private void showCollaborationRemoved(
            InstantMessage message, Context context, MessageDispatcher messageDispatcher) {
        String tabGroupTitle = tabGroupTitleFromMessage();
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
                () -> {});
    }

    private void showGenericMessage(
            MessageDispatcher messageDispatcher,
            @MessageIdentifier int messageIdentifier,
            String title,
            String buttonText,
            Drawable icon,
            Runnable action) {
        Supplier<Integer> onPrimary =
                () -> {
                    action.run();
                    return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
                };
        PropertyModel propertyModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER, messageIdentifier)
                        .with(MessageBannerProperties.TITLE, title)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, buttonText)
                        .with(MessageBannerProperties.ICON, icon)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, onPrimary)
                        .build();
        messageDispatcher.enqueueWindowScopedMessage(propertyModel, /* highPriority= */ false);
    }
}
