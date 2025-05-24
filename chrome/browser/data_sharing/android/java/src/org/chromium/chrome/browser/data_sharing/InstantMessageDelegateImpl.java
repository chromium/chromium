// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
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
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Responsible for displaying browser and OS messages for share. This is effectively a singleton,
 * scoped by profile. This class should be attached/detached by all windows.
 */
@NullMarked
public class InstantMessageDelegateImpl implements InstantMessageDelegate {
    private static class AttachedWindowInfo {
        public final WindowAndroid windowAndroid;
        public final TabGroupModelFilter tabGroupModelFilter;
        public final DataSharingNotificationManager dataSharingNotificationManager;
        public final DataSharingTabManager dataSharingTabManager;
        public final Supplier<Boolean> isActiveWindowSupplier;

        public AttachedWindowInfo(
                WindowAndroid windowAndroid,
                TabGroupModelFilter tabGroupModelFilter,
                DataSharingNotificationManager dataSharingNotificationManager,
                DataSharingTabManager dataSharingTabManager,
                Supplier<Boolean> isActiveWindowSupplier) {
            this.windowAndroid = windowAndroid;
            this.tabGroupModelFilter = tabGroupModelFilter;
            this.dataSharingNotificationManager = dataSharingNotificationManager;
            this.dataSharingTabManager = dataSharingTabManager;
            this.isActiveWindowSupplier = isActiveWindowSupplier;
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
     * @param isActiveWindowSupplier Used to find out the last focused window as a fallback option.
     */
    public void attachWindow(
            WindowAndroid windowAndroid,
            TabGroupModelFilter tabGroupModelFilter,
            DataSharingNotificationManager dataSharingNotificationManager,
            DataSharingTabManager dataSharingTabManager,
            Supplier<Boolean> isActiveWindowSupplier) {
        assert windowAndroid != null;
        assert tabGroupModelFilter != null;
        assert !tabGroupModelFilter.getTabModel().isIncognito();
        assert dataSharingNotificationManager != null;
        assert dataSharingTabManager != null;
        mAttachList.add(
                new AttachedWindowInfo(
                        windowAndroid,
                        tabGroupModelFilter,
                        dataSharingNotificationManager,
                        dataSharingTabManager,
                        isActiveWindowSupplier));
    }

    /**
     * @param windowAndroid The window that is no longer usable for showing messages.
     */
    public void detachWindow(WindowAndroid windowAndroid) {
        assert windowAndroid != null;
        mAttachList.removeIf(awi -> Objects.equals(awi.windowAndroid, windowAndroid));
    }

    @Override
    public void displayInstantaneousMessage(
            InstantMessage message, Callback<Boolean> successCallback) {
        // For TAB_GROUP_REMOVED messages, the group is gone and there is no attached window info.
        // Hence using the last focused window is our best bet.
        boolean fallbackToLastFocusedWindow =
                message.collaborationEvent == CollaborationEvent.TAB_GROUP_REMOVED;

        @Nullable AttachedWindowInfo attachedWindowInfo =
                getAttachedWindowInfo(message, fallbackToLastFocusedWindow);
        if (attachedWindowInfo == null) {
            successCallback.onResult(false);
            return;
        }

        WindowAndroid windowAndroid = attachedWindowInfo.windowAndroid;
        @Nullable Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            successCallback.onResult(false);
            return;
        }

        TabGroupModelFilter tabGroupModelFilter = attachedWindowInfo.tabGroupModelFilter;

        DataSharingTabManager dataSharingTabManager = attachedWindowInfo.dataSharingTabManager;
        @CollaborationEvent int collaborationEvent = message.collaborationEvent;

        if (message.level == InstantNotificationLevel.SYSTEM) {
            if (collaborationEvent == CollaborationEvent.COLLABORATION_MEMBER_ADDED) {

                DataSharingNotificationManager dataSharingNotificationManager =
                        attachedWindowInfo.dataSharingNotificationManager;
                showCollaborationMemberAddedSystemNotification(
                        message, dataSharingNotificationManager);
            }
            successCallback.onResult(true);
        } else if (message.level == InstantNotificationLevel.BROWSER) {

            @Nullable MessageDispatcher messageDispatcher =
                    MessageDispatcherProvider.from(windowAndroid);
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
                        message, activity, messageDispatcher, dataSharingTabManager, onSuccess);
            } else if (collaborationEvent == CollaborationEvent.TAB_GROUP_REMOVED) {
                showCollaborationRemoved(message, activity, messageDispatcher, onSuccess);
            } else {
                // Will never be able to handle this message.
                onSuccess.run();
            }
        }
    }

    @Override
    public void hideInstantaneousMessage(Set<String> messageIds) {
        // TODO(crbug.com/416264627): Implement this.
    }

    private @Nullable AttachedWindowInfo getAttachedWindowInfo(
            InstantMessage message, boolean fallbackToLastFocusedWindow) {
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
            if (!tabGroupModelFilter.tabGroupExists(tabGroupId)) continue;

            return info;
        }

        if (fallbackToLastFocusedWindow) {
            for (AttachedWindowInfo info : mAttachList) {
                if (info.isActiveWindowSupplier.get()) {
                    return info;
                }
            }
        }

        // Tab group was deleted or window not active.
        return null;
    }

    private void fetchAvatarIconFromMessage(
            Context context, @Nullable GroupMember groupMember, Callback<Drawable> onDrawable) {
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
        String buttonText = context.getString(R.string.data_sharing_browser_message_reopen);
        GroupMember groupMember = MessageUtils.extractMember(message);
        Runnable openTabAction = prepareOpenTabActionForRemovedTab(message, tabGroupModelFilter);

        fetchAvatarIconFromMessage(
                context,
                groupMember,
                (icon) -> {
                    showGenericMessage(
                            messageDispatcher,
                            MessageIdentifier.TAB_REMOVED_THROUGH_COLLABORATION,
                            message.localizedMessage,
                            buttonText,
                            icon,
                            openTabAction,
                            onSuccess);
                });
    }

    private Runnable prepareOpenTabActionForRemovedTab(
            InstantMessage message, TabGroupModelFilter tabGroupModelFilter) {
        // Okay to use extractTabGroupId here, as these actions require the tab to be in the current
        // model already.
        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        String url = MessageUtils.extractTabUrl(message);
        return () -> doOpenTab(tabGroupId, url, tabGroupModelFilter);
    }

    private Runnable prepareOpenTabActionForUpdatedTab(
            InstantMessage message, TabGroupModelFilter tabGroupModelFilter) {
        // Okay to use extractTabGroupId here, as these actions require the tab to be in the current
        // model already.
        @Nullable Token tabGroupId = MessageUtils.extractTabGroupId(message);
        String url = MessageUtils.extractPrevTabUrl(message);
        return () -> doOpenTab(tabGroupId, url, tabGroupModelFilter);
    }

    private void doOpenTab(
            @Nullable Token tabGroupId,
            @Nullable String url,
            TabGroupModelFilter tabGroupModelFilter) {
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
        String buttonText = context.getString(R.string.data_sharing_browser_message_reopen);
        GroupMember groupMember = MessageUtils.extractMember(message);
        Runnable openTabAction = prepareOpenTabActionForUpdatedTab(message, tabGroupModelFilter);

        fetchAvatarIconFromMessage(
                context,
                groupMember,
                (icon) -> {
                    showGenericMessage(
                            messageDispatcher,
                            MessageIdentifier.TAB_NAVIGATED_THROUGH_COLLABORATION,
                            message.localizedMessage,
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
            DataSharingTabManager dataSharingTabManager,
            Runnable onSuccess) {
        @Nullable String collaborationId = MessageUtils.extractCollaborationId(message);
        @Nullable String syncId = MessageUtils.extractSyncTabGroupId(message);
        String buttonText = activity.getString(R.string.data_sharing_browser_message_manage);
        GroupMember groupMember = MessageUtils.extractMember(message);
        Runnable openManageSharingRunnable =
                () -> {
                    // TODO(crbug.com/379148260): Use shared #isCollaborationIdValid.
                    if (TextUtils.isEmpty(collaborationId)) return;
                    if (TextUtils.isEmpty(syncId)) return;
                    if (mTabGroupSyncService.getGroup(syncId) == null) return;

                    dataSharingTabManager.createOrManageFlow(
                            EitherGroupId.createSyncId(syncId),
                            CollaborationServiceShareOrManageEntryPoint.ANDROID_MESSAGE,
                            /* createGroupFinishedCallback= */ null);
                };

        fetchAvatarIconFromMessage(
                activity,
                groupMember,
                (icon) -> {
                    showGenericMessage(
                            messageDispatcher,
                            MessageIdentifier.COLLABORATION_MEMBER_ADDED,
                            message.localizedMessage,
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
            Runnable onSuccess) {
        String buttonText = context.getString(R.string.data_sharing_invitation_failure_button);
        Drawable icon = ContextCompat.getDrawable(context, R.drawable.ic_features_24dp);
        showGenericMessage(
                messageDispatcher,
                MessageIdentifier.COLLABORATION_REMOVED,
                message.localizedMessage,
                buttonText,
                icon,
                CallbackUtils.emptyRunnable(),
                onSuccess);
    }

    private void showCollaborationMemberAddedSystemNotification(
            InstantMessage message, DataSharingNotificationManager dataSharingNotificationManager) {
        @Nullable String syncId = MessageUtils.extractSyncTabGroupId(message);
        if (TextUtils.isEmpty(syncId)) return;
        @Nullable SavedTabGroup syncGroup = mTabGroupSyncService.getGroup(syncId);
        if (syncGroup == null) return;

        int notificationId = notificationIdFromMessage(message);
        dataSharingNotificationManager.showOtherJoinedNotification(
                message.localizedMessage, syncGroup.syncId, notificationId);
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
        AtomicReference<PropertyModel> propertyModel = new AtomicReference<>();
        Callback<Boolean> onVisibleChange =
                (fullyVisible) -> {
                    if (fullyVisible) {
                        onSuccess.run();
                    } else {
                        // For shared tab group related messages, we want to show any message only
                        // once. Once a message gets hidden for whatever reason (e.g. timeout, user
                        // switching apps, switching activities, switching to tab switcher etc), we
                        // want to dismiss the message. This is to avoid confusion to the user later
                        // when the message is shown out of context. We use a PostTask here to avoid
                        // a crash that happens since the message dispatcher is still not done
                        // hiding the message before it could process the dismissal.
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    messageDispatcher.dismissMessage(
                                            assumeNonNull(propertyModel.get()),
                                            DismissReason.DISMISSED_BY_FEATURE);
                                });
                    }
                };
        propertyModel.set(
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
                        .build());
        messageDispatcher.enqueueWindowScopedMessage(
                assumeNonNull(propertyModel.get()), /* highPriority= */ false);
    }
}
