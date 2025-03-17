// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.InstantMessage;
import org.chromium.components.collaboration.messaging.InstantNotificationLevel;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.InstantMessageDelegate;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig.DataSharingAvatarCallback;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.UUID;

/**
 * Responsible for displaying browser and OS messages for share. This is effectively a singleton,
 * scoped by profile. This class should be attached/detached by all windows.
 */
public class InstantMessageDelegateImpl implements InstantMessageDelegate {
    private static class AttachedWindowInfo {
        public final WindowAndroid windowAndroid;
        public final TabGroupModelFilter tabGroupModelFilter;
        public final DataSharingNotificationManager dataSharingNotificationManager;
        public final DataSharingTabManager dataSharingTabManager;

        public AttachedWindowInfo(
                WindowAndroid windowAndroid,
                TabGroupModelFilter tabGroupModelFilter,
                DataSharingNotificationManager dataSharingNotificationManager,
                DataSharingTabManager dataSharingTabManager) {
            this.windowAndroid = windowAndroid;
            this.tabGroupModelFilter = tabGroupModelFilter;
            this.dataSharingNotificationManager = dataSharingNotificationManager;
            this.dataSharingTabManager = dataSharingTabManager;
        }
    }

    /**
     * Helper class similar to {@link Runnable}, but only runs the first time it it invoked. After
     * this the reference to the underlying {@link Runnable} is let go, allowing garbage collection.
     * Subsequent invocations are ignored.
     */
    private static class ReuseSafeOnceRunnable implements Runnable {
        private @Nullable Runnable mRunnable;

        public ReuseSafeOnceRunnable(@Nullable Runnable runnable) {
            mRunnable = runnable;
        }

        @Override
        public void run() {
            if (mRunnable != null) {
                mRunnable.run();
                mRunnable = null;
            }
        }
    }

    private final List<AttachedWindowInfo> mAttachList = new ArrayList<>();
    private final DataSharingService mDataSharingService;
    private final TabGroupSyncService mTabGroupSyncService;

    /**
     * @param messagingBackendService Where to register ourself as the current delegate.
     * @param dataSharingService Data sharing service for the profile.
     * @param tabGroupSyncService To access data about tab groups not open in the current model.
     */
    /* package */ InstantMessageDelegateImpl(
            MessagingBackendService messagingBackendService,
            DataSharingService dataSharingService,
            TabGroupSyncService tabGroupSyncService) {
        messagingBackendService.setInstantMessageDelegate(this);
        mDataSharingService = dataSharingService;
        mTabGroupSyncService = tabGroupSyncService;
    }

    /**
     * @param windowAndroid The window that can be used for showing messages.
     * @param tabGroupModelFilter The tab model and group filter for the given window.
     * @param dataSharingNotificationManager Used to send notifications for a particular window.
     * @param dataSharingTabManager Used to display share UI.
     */
    public void attachWindow(
            @NonNull WindowAndroid windowAndroid,
            @NonNull TabGroupModelFilter tabGroupModelFilter,
            @NonNull DataSharingNotificationManager dataSharingNotificationManager,
            @NonNull DataSharingTabManager dataSharingTabManager) {
        assert windowAndroid != null;
        assert tabGroupModelFilter != null;
        assert !tabGroupModelFilter.isIncognito();
        assert dataSharingNotificationManager != null;
        assert dataSharingTabManager != null;
        mAttachList.add(
                new AttachedWindowInfo(
                        windowAndroid,
                        tabGroupModelFilter,
                        dataSharingNotificationManager,
                        dataSharingTabManager));
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
        @Nullable Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            successCallback.onResult(false);
            return;
        }

        @NonNull TabGroupModelFilter tabGroupModelFilter = attachedWindowInfo.tabGroupModelFilter;
        @NonNull
        DataSharingTabManager dataSharingTabManager = attachedWindowInfo.dataSharingTabManager;
        @CollaborationEvent int collaborationEvent = message.collaborationEvent;

        if (message.level == InstantNotificationLevel.SYSTEM) {
            if (collaborationEvent == CollaborationEvent.COLLABORATION_MEMBER_ADDED) {
                @NonNull
                DataSharingNotificationManager dataSharingNotificationManager =
                        attachedWindowInfo.dataSharingNotificationManager;
                showCollaborationMemberAddedSystemNotification(
                        message, activity, dataSharingNotificationManager, tabGroupModelFilter);
            }
            successCallback.onResult(true);
        } else if (message.level == InstantNotificationLevel.BROWSER) {
            @Nullable
            MessageDispatcher messageDispatcher = MessageDispatcherProvider.from(windowAndroid);
            if (messageDispatcher == null) {
                successCallback.onResult(false);
                return;
            }

            ReuseSafeOnceRunnable onSuccess = new ReuseSafeOnceRunnable(successCallback.bind(true));
            if (collaborationEvent == CollaborationEvent.TAB_REMOVED) {
                showTabRemoved(
                        message, activity, messageDispatcher, tabGroupModelFilter, onSuccess);
            } else if (collaborationEvent == CollaborationEvent.TAB_UPDATED) {
                showTabChange(message, activity, messageDispatcher, tabGroupModelFilter, onSuccess);
            } else if (collaborationEvent == CollaborationEvent.COLLABORATION_MEMBER_ADDED) {
                showCollaborationMemberAdded(
                        message,
                        activity,
                        messageDispatcher,
                        tabGroupModelFilter,
                        dataSharingTabManager,
                        onSuccess);
            } else if (collaborationEvent == CollaborationEvent.TAB_GROUP_REMOVED) {
                showCollaborationRemoved(
                        message, activity, messageDispatcher, tabGroupModelFilter, onSuccess);
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
            int rootId = tabGroupModelFilter.getRootIdFromTabGroupId(tabGroupId);
            if (rootId == Tab.INVALID_TAB_ID) continue;

            // If we had a valid rootId, this is the right window.
            return info;
        }

        // Tab group was deleted or window not active.
        return null;
    }

    private void fetchAvatarIconFromMessage(
            Context context, GroupMember groupMember, Callback<Drawable> onDrawable) {
        DataSharingAvatarCallback onBitmap =
                (Bitmap bitmap) -> onDrawable.onResult(new BitmapDrawable(bitmap));
        int sizeInPixels =
                context.getResources().getDimensionPixelSize(R.dimen.message_description_icon_size);
        @ColorInt int fallbackColor = SemanticColorUtils.getDefaultIconColorAccent1(context);
        DataSharingAvatarBitmapConfig config =
                new DataSharingAvatarBitmapConfig.Builder()
                        .setContext(context)
                        .setGroupMember(groupMember)
                        .setIsDarkMode(ColorUtils.inNightMode(context))
                        .setAvatarSizeInPixels(sizeInPixels)
                        .setAvatarFallbackColor(fallbackColor)
                        .setDataSharingAvatarCallback(onBitmap)
                        .build();
        mDataSharingService.getUiDelegate().getAvatarBitmap(config);
    }

    private void showTabRemoved(
            InstantMessage message,
            Context context,
            MessageDispatcher messageDispatcher,
            TabGroupModelFilter tabGroupModelFilter,
            Runnable onSuccess) {
        String givenName = MessageUtils.extractGivenName(message);
        String tabTitle = MessageUtils.extractTabTitle(message);
        String title =
                context.getString(
                        R.string.data_sharing_browser_message_removed_tab, givenName, tabTitle);
        String buttonText = context.getString(R.string.data_sharing_browser_message_reopen);
        GroupMember groupMember = MessageUtils.extractMember(message);
        Runnable openTabAction = prepareOpenTabAction(message, tabGroupModelFilter);

        fetchAvatarIconFromMessage(
                context,
                groupMember,
                (icon) -> {
                    showGenericMessage(
                            messageDispatcher,
                            MessageIdentifier.TAB_REMOVED_THROUGH_COLLABORATION,
                            title,
                            buttonText,
                            icon,
                            openTabAction,
                            onSuccess);
                });
    }

    private Runnable prepareOpenTabAction(
            InstantMessage message, TabGroupModelFilter tabGroupModelFilter) {
        // Okay to use extractTabGroupId here, as these actions require the tab to be in the current
        // model already.
        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        String url = MessageUtils.extractTabUrl(message);
        return () -> doOpenTab(tabGroupId, url, tabGroupModelFilter);
    }

    private void doOpenTab(Token tabGroupId, String url, TabGroupModelFilter tabGroupModelFilter) {
        url = TextUtils.isEmpty(url) ? UrlConstants.NTP_URL : url;
        int rootId = tabGroupModelFilter.getRootIdFromTabGroupId(tabGroupId);
        TabGroupUtils.openUrlInGroup(
                tabGroupModelFilter, url, rootId, TabLaunchType.FROM_TAB_GROUP_UI);
    }

    private void showTabChange(
            InstantMessage message,
            Context context,
            MessageDispatcher messageDispatcher,
            TabGroupModelFilter tabGroupModelFilter,
            Runnable onSuccess) {
        String givenName = MessageUtils.extractGivenName(message);
        String tabTitle = MessageUtils.extractTabTitle(message);
        String title =
                context.getString(
                        R.string.data_sharing_browser_message_changed_tab, givenName, tabTitle);
        String buttonText = context.getString(R.string.data_sharing_browser_message_reopen);
        GroupMember groupMember = MessageUtils.extractMember(message);
        Runnable openTabAction = prepareOpenTabAction(message, tabGroupModelFilter);

        fetchAvatarIconFromMessage(
                context,
                groupMember,
                (icon) -> {
                    showGenericMessage(
                            messageDispatcher,
                            MessageIdentifier.TAB_NAVIGATED_THROUGH_COLLABORATION,
                            title,
                            buttonText,
                            icon,
                            openTabAction,
                            onSuccess);
                });
    }

    private void showCollaborationMemberAdded(
            InstantMessage message,
            Activity activity,
            MessageDispatcher messageDispatcher,
            TabGroupModelFilter tabGroupModelFilter,
            DataSharingTabManager dataSharingTabManager,
            Runnable onSuccess) {
        @Nullable String collaborationId = MessageUtils.extractCollaborationId(message);
        String givenName = MessageUtils.extractGivenName(message);
        String tabGroupTitle = getTabGroupTitle(message, activity, tabGroupModelFilter);
        String title =
                activity.getString(
                        R.string.data_sharing_browser_message_joined_tab_group,
                        givenName,
                        tabGroupTitle);
        String syncId = MessageUtils.extractSyncTabGroupId(message);
        Token localId = MessageUtils.extractTabGroupId(message);
        String buttonText = activity.getString(R.string.data_sharing_browser_message_manage);
        GroupMember groupMember = MessageUtils.extractMember(message);
        Runnable openManageSharingRunnable =
                () -> {
                    // TODO(crbug.com/379148260): Use shared #isCollaborationIdValid.
                    if (TextUtils.isEmpty(collaborationId)) return;
                    if (mTabGroupSyncService.getGroup(syncId) == null) return;

                    dataSharingTabManager.createOrManageFlow(
                            activity,
                            syncId,
                            new LocalTabGroupId(localId),
                            /* createGroupFinishedCallback= */ null);
                };

        fetchAvatarIconFromMessage(
                activity,
                groupMember,
                (icon) -> {
                    showGenericMessage(
                            messageDispatcher,
                            MessageIdentifier.COLLABORATION_MEMBER_ADDED,
                            title,
                            buttonText,
                            icon,
                            openManageSharingRunnable,
                            onSuccess);
                });
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
                CallbackUtils.emptyRunnable(),
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

        String syncId = MessageUtils.extractSyncTabGroupId(message);
        @Nullable SavedTabGroup syncGroup = mTabGroupSyncService.getGroup(syncId);
        if (syncGroup == null) return;

        int notificationId = notificationIdFromMessage(message);
        dataSharingNotificationManager.showOtherJoinedNotification(
                contentTitle, syncGroup.syncId, notificationId);
    }

    private static int notificationIdFromMessage(InstantMessage message) {
        String messageId = MessageUtils.extractMessageId(message);
        // Even on failure, we'll still be okay. Messages will just always override.
        if (messageId == null) return 0;
        try {
            // Consistently grab the same 32 bits.
            UUID uuid = UUID.fromString(messageId);
            return (int) uuid.getLeastSignificantBits();
        } catch (IllegalArgumentException iae) {
            return 0;
        }
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
                        .with(
                                MessageBannerProperties.ICON_ROUNDED_CORNER_RADIUS_PX,
                                icon.getIntrinsicWidth())
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, onPrimary)
                        .with(MessageBannerProperties.ON_FULLY_VISIBLE, onVisibleChange)
                        .build();
        messageDispatcher.enqueueWindowScopedMessage(propertyModel, /* highPriority= */ false);
    }

    private String getTabGroupTitle(
            InstantMessage message, Context context, TabGroupModelFilter tabGroupModelFilter) {
        String messageTitle = MessageUtils.extractTabGroupTitle(message);
        if (TextUtils.isEmpty(messageTitle)) {
            @Nullable String syncId = MessageUtils.extractSyncTabGroupId(message);
            @Nullable SavedTabGroup syncGroup = mTabGroupSyncService.getGroup(syncId);
            @Nullable Token token = extractLocalId(syncGroup);
            int rootId = tabGroupModelFilter.getRootIdFromTabGroupId(token);
            int tabCount = tabGroupModelFilter.getRelatedTabCountForRootId(rootId);
            return TabGroupTitleUtils.getDefaultTitle(context, tabCount);
        } else {
            return messageTitle;
        }
    }

    private @Nullable Token extractLocalId(@Nullable SavedTabGroup syncGroup) {
        return syncGroup == null || syncGroup.localId == null ? null : syncGroup.localId.tabGroupId;
    }
}
