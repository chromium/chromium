// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Manages all SendTabToSelf related notifications for Android. This includes displaying, handling
 * taps, and timeouts.
 */
public class NotificationManager {
    private static final String NOTIFICATION_GUID_EXTRA = "send_tab_to_self.notification.guid";
    // Action constants for the registered BroadcastReceiver.
    private static final String NOTIFICATION_ACTION_TAP = "send_tab_to_self.tap";
    private static final String NOTIFICATION_ACTION_DISMISS = "send_tab_to_self.dismiss";
    private static final String NOTIFICATION_ACTION_TIMEOUT = "send_tab_to_self.timeout";

    /**
     * Open the URL specified within Chrome.
     *
     * @param uri The URI to open.
     */
    private static void openUrl(Uri uri) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(uri)
                        .setClass(context, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        context.startActivity(intent);
    }

    public static void handleIntent(Intent intent) {
        final String action = intent.getAction();
        final String guid = IntentUtils.safeGetStringExtra(intent, NOTIFICATION_GUID_EXTRA);
        // If this feature ever supports incognito mode, we need to modify
        // this method to obtain the current profile, rather than the last-used
        // regular profile.
        final Profile profile = ProfileManager.getLastUsedRegularProfile();
        switch (action) {
            case NOTIFICATION_ACTION_TAP:
                openUrl(intent.getData());
                hideNotification(guid);
                SendTabToSelfAndroidBridge.deleteEntry(profile, guid);
                MetricsRecorder.recordNotificationOpened();
                break;
            case NOTIFICATION_ACTION_DISMISS:
                hideNotification(guid);
                SendTabToSelfAndroidBridge.dismissEntry(profile, guid);
                MetricsRecorder.recordNotificationDismissed();
                break;
            case NOTIFICATION_ACTION_TIMEOUT:
                SendTabToSelfAndroidBridge.dismissEntry(profile, guid);
                MetricsRecorder.recordNotificationTimedOut();
                break;
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
        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create(context);
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
    private static boolean showNotification(
            String guid,
            @NonNull String url,
            String title,
            String deviceName,
            long timeoutAtMillis,
            Class<? extends BroadcastReceiver> broadcastReceiver) {
        // A notification associated with this Share entry already exists. Don't display a new one.
        if (NotificationSharedPrefManager.findActiveNotification(guid) != null) {
            return false;
        }

        // Post notification.
        Context context = ContextUtils.getApplicationContext();
        BaseNotificationManagerProxy manager = BaseNotificationManagerProxyFactory.create(context);

        int nextId = NotificationSharedPrefManager.getNextNotificationId();
        Uri uri = Uri.parse(url);
        PendingIntentProvider contentIntent =
                PendingIntentProvider.getBroadcast(
                        context,
                        nextId,
                        new Intent(context, broadcastReceiver)
                                .setData(uri)
                                .setAction(NOTIFICATION_ACTION_TAP)
                                .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                        0);
        PendingIntentProvider deleteIntent =
                PendingIntentProvider.getBroadcast(
                        context,
                        nextId,
                        new Intent(context, broadcastReceiver)
                                .setData(uri)
                                .setAction(NOTIFICATION_ACTION_DISMISS)
                                .putExtra(NOTIFICATION_GUID_EXTRA, guid),
                        0);
        // IDS_SEND_TAB_TO_SELF_NOTIFICATION_CONTEXT_TEXT
        Resources res = context.getResources();
        String contextText =
                res.getString(
                        R.string.send_tab_to_self_notification_context_text,
                        uri.getHost(),
                        deviceName);
        // Build the notification itself.
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.SHARING,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType
                                                .SEND_TAB_TO_SELF,
                                        NotificationConstants.GROUP_SEND_TAB_TO_SELF,
                                        nextId))
                        .setContentIntent(contentIntent)
                        .setDeleteIntent(deleteIntent)
                        .setContentTitle(title)
                        .setContentText(contextText)
                        .setGroup(NotificationConstants.GROUP_SEND_TAB_TO_SELF)
                        .setPriorityBeforeO(NotificationCompat.PRIORITY_HIGH)
                        .setVibrate(new long[0])
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setDefaults(Notification.DEFAULT_ALL);
        NotificationWrapper notification = builder.buildNotificationWrapper();

        manager.notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.SEND_TAB_TO_SELF,
                        notification.getNotification());

        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(nextId, guid));

        // Set timeout.
        if (timeoutAtMillis != Long.MAX_VALUE) {
            AlarmManager alarmManager =
                    (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
            Intent timeoutIntent =
                    new Intent(context, broadcastReceiver)
                            .setData(Uri.parse(url))
                            .setAction(NOTIFICATION_ACTION_TIMEOUT)
                            .putExtra(NOTIFICATION_GUID_EXTRA, guid);
            alarmManager.set(
                    AlarmManager.RTC,
                    timeoutAtMillis,
                    PendingIntent.getBroadcast(
                            context,
                            nextId,
                            timeoutIntent,
                            PendingIntent.FLAG_UPDATE_CURRENT
                                    | IntentUtils.getPendingIntentMutabilityFlag(false)));
        }
        MetricsRecorder.recordNotificationShown();
        return true;
    }
}
