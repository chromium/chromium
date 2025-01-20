// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Sends notification for information update of Data Sharing service to user. */
public class DataSharingNotificationManager {
    private final Context mContext;
    private final BaseNotificationManagerProxy mNotificationManagerProxy;
    private static final String TAG = "data_sharing";
    // TODO(b/329155961): Use the collaboration_id given by data sharing service.
    private static final int NOTIFICATION_ID = 5000;
    public static final String ACTION_EXTRA = "org.chromium.chrome.browser.data_sharing.action";
    public static final String INVITATION_URL_EXTRA =
            "org.chromium.chrome.browser.data_sharing.invitation_url";
    public static final String TAB_GROUP_SYNC_ID_EXTRA =
            "org.chromium.chrome.browser.data_sharing.tab_group_sync_id";

    @IntDef({Action.UNKNOWN, Action.INVITATION_FLOW, Action.MANAGE_TAB_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    /* package */ @interface Action {
        /** Maybe from a parcelling error, expected to be no-oped. */
        int UNKNOWN = 0;

        /** Starts the invitation flow after opening the tab switcher. */
        int INVITATION_FLOW = 1;

        /** Opens the tab group dialog inside the tab switcher for the given tab group. */
        int MANAGE_TAB_GROUP = 2;
    }

    /** Receive data sharing notification click event. */
    public static final class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            @Action int action = IntentUtils.safeGetIntExtra(intent, ACTION_EXTRA, Action.UNKNOWN);
            if (action == Action.INVITATION_FLOW) {
                Intent invitationIntent = createInvitationIntent(context, GURL.emptyGURL());
                IntentUtils.safeStartActivity(context, invitationIntent);
            } else if (action == Action.MANAGE_TAB_GROUP) {
                String syncId = IntentUtils.safeGetStringExtra(intent, TAB_GROUP_SYNC_ID_EXTRA);
                Intent manageIntent = createManageIntent(context, syncId);
                IntentUtils.safeStartActivity(context, manageIntent);
            }
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
        Intent launchIntent = createdIntentShared(context, Action.INVITATION_FLOW);
        launchIntent.putExtra(INVITATION_URL_EXTRA, url.getSpec());
        return launchIntent;
    }

    private static Intent createManageIntent(Context context, String syncId) {
        Intent launchIntent = createdIntentShared(context, Action.MANAGE_TAB_GROUP);
        launchIntent.putExtra(TAB_GROUP_SYNC_ID_EXTRA, syncId);
        return launchIntent;
    }

    private static Intent createdIntentShared(Context context, @Action int action) {
        Intent launchIntent = new Intent(Intent.ACTION_VIEW);
        launchIntent.addCategory(Intent.CATEGORY_DEFAULT);
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        launchIntent.setClassName(context, BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME);
        launchIntent.putExtra(ACTION_EXTRA, action);
        IntentUtils.addTrustedIntentExtras(launchIntent);
        return launchIntent;
    }

    public DataSharingNotificationManager(Context context) {
        mContext = context;
        mNotificationManagerProxy = BaseNotificationManagerProxyFactory.create();
    }

    @VisibleForTesting
    protected NotificationWrapperBuilder getNotificationBuilder() {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChromeChannelDefinitions.ChannelId.COLLABORATION,
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
        Intent pendingIntent = createPendingIntent(Action.INVITATION_FLOW);
        buildAndNotify(contentTitle, /* showWhen= */ false, pendingIntent);
    }

    /**
     * Shows a notification that another user joined a collaboration.
     *
     * @param contentTitle The text to display.
     * @param syncId The sync id of the tab group that should be opened upon action interaction.
     */
    public void showOtherJoinedNotification(String contentTitle, String syncId) {
        Intent pendingIntent = createPendingIntent(Action.MANAGE_TAB_GROUP);
        pendingIntent.putExtra(TAB_GROUP_SYNC_ID_EXTRA, syncId);
        buildAndNotify(contentTitle, /* showWhen= */ true, pendingIntent);
    }

    private void buildAndNotify(String contentTitle, boolean showWhen, Intent pendingIntent) {
        PendingIntentProvider pendingIntentProvider =
                PendingIntentProvider.getBroadcast(
                        mContext,
                        /* requestCode= */ 0,
                        pendingIntent,
                        PendingIntent.FLAG_IMMUTABLE);

        NotificationWrapper notification =
                getNotificationBuilder()
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
