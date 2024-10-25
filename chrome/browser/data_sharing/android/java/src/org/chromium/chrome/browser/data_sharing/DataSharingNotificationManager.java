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
import org.chromium.base.Token;
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
            Intent invitationIntent = createInvitationIntent(context, GURL.emptyGURL());
            IntentUtils.safeStartActivity(context, invitationIntent);
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
        Intent launchIntent = new Intent(Intent.ACTION_VIEW);
        launchIntent.addCategory(Intent.CATEGORY_DEFAULT);
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        launchIntent.setClassName(context, BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME);
        launchIntent.putExtra(DATA_SHARING_EXTRA, url.getSpec());
        IntentUtils.addTrustedIntentExtras(launchIntent);
        return launchIntent;
    }

    public DataSharingNotificationManager(Context context) {
        mContext = context;
        mNotificationManagerProxy =
                BaseNotificationManagerProxyFactory.create(context.getApplicationContext());
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

    /**
     * Shows a notification for being invited to a new collaboration.
     *
     * @param displayName The name to display for the inviting user.
     */
    public void showInvitationFlowNotification(String displayName) {
        String contentTitle =
                mContext.getString(
                        R.string.data_sharing_invitation_notification_title, displayName);
        buildAndNotify(contentTitle, /* showWhen= */ false);
    }

    /**
     * Shows a notification that another user joined a collaboration.
     *
     * @param contentTitle The text to display.
     * @param tabGroupId The id of the tab group that should be opened upon action interaction.
     */
    public void showOtherJoinedNotification(String contentTitle, Token tabGroupId) {
        // TODO(https://crbug.com/369186228): Add tabGroupId as an extra.
        buildAndNotify(contentTitle, /* showWhen= */ true);
    }

    private void buildAndNotify(String contentTitle, boolean showWhen) {
        NotificationWrapper notification =
                getNotificationBuilder()
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setShowWhen(showWhen)
                        .setAutoCancel(true)
                        .setLocalOnly(true)
                        .setContentTitle(contentTitle)
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
