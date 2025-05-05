// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.chromium.chrome.browser.data_sharing.DataSharingIntentUtils.ACTION_EXTRA;
import static org.chromium.chrome.browser.data_sharing.DataSharingIntentUtils.TAB_GROUP_SYNC_ID_EXTRA;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.DataSharingIntentUtils.Action;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.url.GURL;

/** Sends notification for information update of Data Sharing service to user. */
@NullMarked
public class DataSharingNotificationManager {
    private final Context mContext;
    private final BaseNotificationManagerProxy mNotificationManagerProxy;
    private static final String TAG = "data_sharing";
    // TODO(b/329155961): Use the collaboration_id given by data sharing service.
    private static final int NOTIFICATION_ID = 5000;

    /** Receive data sharing notification click event. */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            @Action int action = IntentUtils.safeGetIntExtra(intent, ACTION_EXTRA, Action.UNKNOWN);
            if (action == Action.INVITATION_FLOW) {
                Intent invitationIntent =
                        DataSharingIntentUtils.createInvitationIntent(context, GURL.emptyGURL());
                IntentUtils.safeStartActivity(context, invitationIntent);
            } else if (action == Action.MANAGE_TAB_GROUP) {
                String syncId = IntentUtils.safeGetStringExtra(intent, TAB_GROUP_SYNC_ID_EXTRA);
                Intent manageIntent = DataSharingIntentUtils.createManageIntent(context, syncId);
                IntentUtils.safeStartActivity(context, manageIntent);
            }
        }
    }

    public DataSharingNotificationManager(Context context) {
        mContext = context;
        mNotificationManagerProxy = BaseNotificationManagerProxyFactory.create();
    }

    @VisibleForTesting
    protected NotificationWrapperBuilder notificationBuilder(int notificationId) {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChromeChannelDefinitions.ChannelId.COLLABORATION,
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                        TAG,
                        notificationId));
    }

    /**
     * Shows a notification for being invited to a new collaboration.
     *
     * @param displayName The name to display for the inviting user.
     */
    public void showInvitationFlowNotification(String displayName) {
        String contentTitle =
                mContext.getString(
                        R.string.data_sharing_invitation_notification_title, displayName);
        Intent pendingIntent = createPendingIntent(Action.INVITATION_FLOW);
        buildAndNotify(contentTitle, /* showWhen= */ false, NOTIFICATION_ID, pendingIntent);
    }

    /**
     * Shows a notification that another user joined a collaboration.
     *
     * @param contentTitle The text to display.
     * @param syncId The sync id of the tab group that should be opened upon action interaction.
     */
    public void showOtherJoinedNotification(
            String contentTitle, @Nullable String syncId, int notificationId) {
        Intent pendingIntent = createPendingIntent(Action.MANAGE_TAB_GROUP);
        pendingIntent.putExtra(TAB_GROUP_SYNC_ID_EXTRA, syncId);
        buildAndNotify(contentTitle, /* showWhen= */ true, notificationId, pendingIntent);
    }

    private void buildAndNotify(
            String contentTitle, boolean showWhen, int notificationId, Intent pendingIntent) {
        PendingIntentProvider pendingIntentProvider =
                PendingIntentProvider.getBroadcast(
                        mContext,
                        /* requestCode= */ 0,
                        pendingIntent,
                        PendingIntent.FLAG_IMMUTABLE);

        NotificationWrapper notification =
                notificationBuilder(notificationId)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setShowWhen(showWhen)
                        .setAutoCancel(true)
                        .setLocalOnly(true)
                        .setContentTitle(contentTitle)
                        .setContentIntent(pendingIntentProvider)
                        .buildNotificationWrapper();

        mNotificationManagerProxy.notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                        notification.getNotification());
    }

    private Intent createPendingIntent(@Action int action) {
        Intent intent = new Intent(mContext, DataSharingNotificationManager.Receiver.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        intent.putExtra(ACTION_EXTRA, action);
        return intent;
    }
}
