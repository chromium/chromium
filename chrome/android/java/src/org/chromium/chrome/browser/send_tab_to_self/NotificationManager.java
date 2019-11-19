// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.provider.Browser;
import android.support.v4.app.NotificationCompat;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareNotificationInteraction;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareNotificationInteraction.InteractionType;

/**
 * Manages all SendTabToSelf related notifications for Android. This includes displaying, handling
 * taps, and timeouts.
 */
public class NotificationManager {
    private static final String NOTIFICATION_GUID_EXTRA = "send_tab_to_self.notification.guid";

    /** Records dismissal when notification is swiped away. */
    public static final class DeleteReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String guid = intent.getStringExtra(NOTIFICATION_GUID_EXTRA);
            hideNotification(guid, InteractionType.DISMISSED);
            SendTabToSelfAndroidBridge.dismissEntry(Profile.getLastUsedProfile(), guid);
        }
    }

    /** Handles the tapping of a notification by opening the URL and hiding the notification. */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            openUrl(intent.getData());
            String guid = intent.getStringExtra(NOTIFICATION_GUID_EXTRA);
            hideNotification(guid, InteractionType.OPENED);
            SendTabToSelfAndroidBridge.deleteEntry(Profile.getLastUsedProfile(), guid);
        }
    }

    /** Removes the notification after a timeout period. */
    public static final class TimeoutReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            String guid = intent.getStringExtra(NOTIFICATION_GUID_EXTRA);
            SendTabToSelfAndroidBridge.dismissEntry(Profile.getLastUsedProfile(), guid);
        }
    }

    /**
     * Open the URL specified within Chrome.
     *
     * @param uri The URI to open.
     */
    private static void openUrl(Uri uri) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent()
                                .setAction(Intent.ACTION_VIEW)
                                .setData(uri)
                                .setClass(context, ChromeLauncherActivity.class)
                                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                                .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName())
                                .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }

    /**
     * Hides a notification and records an action to the Actions histogram.
     * <p>
     * If the notification is not actually visible, then no action will be taken, and the action
     * will not be recorded.
     *
     * @param guid The GUID of the notification to hide.
     */
    private static void hideNotification(@Nullable String guid, @InteractionType int type) {
        if (hideNotification(guid)) {
            SendTabToSelfShareNotificationInteraction.recordClickResult(type);
        }
    }

    /**
     * Hides a notification.
     *
     * @param guid The GUID of the notification to hide.
     * @return whether the notification was hidden. False if there is corresponding notification to
     * hide.
     */
    @CalledByNative
    private static boolean hideNotification(@Nullable String guid) {
        NotificationSharedPrefManager.ActiveNotification activeNotification =
                NotificationSharedPrefManager.findActiveNotification(guid);
        if (!NotificationSharedPrefManager.removeActiveNotification(guid)) {
            return false;
        }
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);
        manager.cancel(
                NotificationConstants.GROUP_SEND_TAB_TO_SELF, activeNotification.notificationId);
        return true;
    }

    /**
     * Displays a notification.
     *
     * @param url URL to open when the user taps on the notification.
     * @param title Title to display within the notification.
     * @param timeoutAtMillis Specifies how long until the notification should be automatically
     *            hidden.
     * @return whether the notification was successfully displayed
     */
    @CalledByNative
    private static boolean showNotification(String guid, @NonNull String url, String title,
            String deviceName, long timeoutAtMillis) {
        // A notification associated with this Share entry already exists. Don't display a new one.
        if (NotificationSharedPrefManager.findActiveNotification(guid) != null) {
            return false;
        }

        // Post notification.
        SendTabToSelfShareNotificationInteraction.recordClickResult(
                SendTabToSelfShareNotificationInteraction.InteractionType.SHOWN);
        Context context = ContextUtils.getApplicationContext();
        NotificationManagerProxy manager = new NotificationManagerProxyImpl(context);

        int nextId = NotificationSharedPrefManager.getNextNotificationId();
        Uri uri = Uri.parse(url);
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context, nextId,
                new Intent(context, TapReceiver.class)
                        .setData(uri)
                        .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                0);
        PendingIntentProvider deleteIntent = PendingIntentProvider.getBroadcast(context, nextId,
                new Intent(context, DeleteReceiver.class)
                        .setData(uri)
                        .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                0);
        // IDS_SEND_TAB_TO_SELF_NOTIFICATION_CONTEXT_TEXT
        Resources res = context.getResources();
        String contextText = res.getString(
                R.string.send_tab_to_self_notification_context_text, uri.getHost(), deviceName);
        // Build the notification itself.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChannelDefinitions.ChannelId.SHARING,
                                null /* remoteAppPackageName */,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType
                                                .SEND_TAB_TO_SELF,
                                        NotificationConstants.GROUP_SEND_TAB_TO_SELF, nextId))
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(deleteIntent)
                        .setContentTitle(title)
                        .setContentText(contextText)
                        .setGroup(NotificationConstants.GROUP_SEND_TAB_TO_SELF)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setVibrate(new long[0])
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDefaults(Notification.DEFAULT_ALL);
        ChromeNotification notification = builder.buildChromeNotification();

        manager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SEND_TAB_TO_SELF,
                notification.getNotification());

        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(nextId, guid));

        // Set timeout.
        if (timeoutAtMillis != Long.MAX_VALUE) {
            AlarmManager alarmManager =
                    (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
            Intent timeoutIntent = new Intent(context, TimeoutReceiver.class)
                                           .setData(Uri.parse(url))
                                           .putExtra(NOTIFICATION_GUID_EXTRA, guid);
            alarmManager.set(AlarmManager.RTC, timeoutAtMillis,
                    PendingIntent.getBroadcast(
                            context, nextId, timeoutIntent, PendingIntent.FLAG_UPDATE_CURRENT));
        }
        return true;
    }
}
