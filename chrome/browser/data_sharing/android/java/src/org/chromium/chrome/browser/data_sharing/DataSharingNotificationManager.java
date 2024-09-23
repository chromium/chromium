// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
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
public class DataSharingNotificationManager {
    private final Context mContext;
    private final BaseNotificationManagerProxy mNotificationManagerProxy;
    private static final String TAG = "data_sharing";
    // TODO(b/329155961): Use the collaboration_id given by data sharing service.
    private static final int NOTIFICATION_ID = 5000;
    public static final String DATA_SHARING_EXTRA = "org.chromium.chrome.browser.data_sharing";

    /** Receive data sharing notification click event. */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // Launch tab switcher view.
            // TODO(b/329155961): Introduce a custom action for all notifications launching from
            // Data Sharing Service.
            Intent invitation_intent = createInvitationIntent(context, GURL.emptyGURL());
            IntentUtils.safeStartActivity(context, invitation_intent);
        }
    }

    /**
     * Create an intent to launch the invitation flow.
     *
     * @param context The {@link Context} to use.
     * @param url The URL associated with the invitation.
     * @return The {@link Intent} to launch the invitation flow.
     */
    public static Intent createInvitationIntent(Context context, GURL url) {
        Intent launch_intent = new Intent(Intent.ACTION_VIEW);
        launch_intent.addCategory(Intent.CATEGORY_DEFAULT);
        launch_intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        launch_intent.setClassName(context, BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME);
        launch_intent.putExtra(DATA_SHARING_EXTRA, url.getSpec());
        IntentUtils.addTrustedIntentExtras(launch_intent);
        return launch_intent;
    }

    @VisibleForTesting
    DataSharingNotificationManager(Context context, BaseNotificationManagerProxy manager) {
        mContext = context;
        mNotificationManagerProxy = manager;
    }

    public DataSharingNotificationManager(Context context) {
        this(context, BaseNotificationManagerProxyFactory.create(context));
    }

    @VisibleForTesting
    protected NotificationWrapperBuilder getNotificationBuilder() {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChromeChannelDefinitions.ChannelId.BROWSER,
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                        TAG,
                        NOTIFICATION_ID));
    }

    /** Show a data sharing notification. */
    public void showNotification(String sharingOrigin) {
        String notificationText =
                mContext.getResources()
                        .getString(
                                R.string.data_sharing_invitation_notification_title, sharingOrigin);

        NotificationWrapper notification =
                getNotificationBuilder()
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setShowWhen(false)
                        .setAutoCancel(true)
                        .setLocalOnly(true)
                        // TODO(b/329155961): Remove temporary strings.
                        .setContentTitle(notificationText)
                        .setContentText(notificationText)
                        .setContentIntent(createIntent(mContext))
                        .buildNotificationWrapper();

        mNotificationManagerProxy.notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.DATA_SHARING,
                        notification.getNotification());
    }

    private static PendingIntentProvider createIntent(Context context) {
        Intent intent = new Intent(context, DataSharingNotificationManager.Receiver.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        return PendingIntentProvider.getBroadcast(
                context, /* requestCode= */ 0, intent, PendingIntent.FLAG_IMMUTABLE);
    }
}
