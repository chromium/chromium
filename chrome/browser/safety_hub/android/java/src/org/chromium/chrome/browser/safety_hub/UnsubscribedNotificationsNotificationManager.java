// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy.StatusBarNotificationProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

import java.util.List;

/** Shows the Safety Hub notification about revoked notification permissions. */
@NullMarked
public class UnsubscribedNotificationsNotificationManager {
    private static final String ACTION_ACK =
            "org.chromium.chrome.browser.safety_hub.NOTIFICATION_ACTION_ACK";

    private static final String TAG = "safety_hub";

    public static final class NotificationReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final @Nullable String action = intent.getAction();
            if (action == null) return;
            switch (action) {
                case ACTION_ACK:
                    UnsubscribedNotificationsNotificationManager.dismissNotification();
                    break;
            }
        }
    }

    private static NotificationWrapperBuilder createNotificationBuilder() {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                ChannelId.BROWSER,
                new NotificationMetadata(
                        NotificationUmaTracker.SystemNotificationType
                                .SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS,
                        TAG,
                        NotificationConstants
                                .NOTIFICATION_ID_SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS));
    }

    private static boolean isDisruptiveNotificationRevocationEnabled() {
        return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION)
                && !ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION,
                        "shadow_run",
                        false);
    }

    private static long getNotificationTimeout() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SAFETY_HUB_DISRUPTIVE_NOTIFICATION_REVOCATION,
                        "notification_timeout_seconds",
                        7 * 24 * 3600)
                * 1000L;
    }

    /**
     * Displays, updates or dismisses the notification about revoked subscriptions from Safety Hub.
     *
     * @param numRevokedPermissions is the number of permissions revoked. If 0, the notification is
     *     dismissed.
     */
    @CalledByNative
    static void displayNotification(@JniType("int32_t") int numRevokedPermissions) {
        assert numRevokedPermissions >= 0
                : "This function expects a non-negative parameter numRevokedPermissions";
        if (numRevokedPermissions <= 0) {
            dismissNotification();
            return;
        }
        displayOrUpdateNotification(numRevokedPermissions, TimeUtils.currentTimeMillis());
    }

    private static void displayOrUpdateNotification(int numRevokedPermissions, long when) {
        if (!isDisruptiveNotificationRevocationEnabled()) {
            dismissNotification();
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();
        String title =
                res.getString(R.string.safety_hub_unsubscribed_notifications_notification_title);
        String contents =
                res.getQuantityString(
                        R.plurals.safety_hub_unsubscribed_notifications_notification_message,
                        numRevokedPermissions,
                        numRevokedPermissions);

        PendingIntentProvider ackIntentProvider =
                PendingIntentProvider.getBroadcast(
                        context,
                        /* requestCode= */ 0,
                        new Intent(context, NotificationReceiver.class).setAction(ACTION_ACK),
                        PendingIntent.FLAG_UPDATE_CURRENT);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent settingsIntent =
                settingsNavigation.createSettingsIntent(
                        context, SafetyHubPermissionsFragment.class);

        PendingIntentProvider settingsIntentProvider =
                PendingIntentProvider.getActivity(
                        context, 0, settingsIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        String ack =
                context.getString(R.string.safety_hub_unsubscribed_notifications_notification_ack);
        String review =
                context.getString(
                        R.string.safety_hub_unsubscribed_notifications_notification_review);

        long notificationTimeout = getNotificationTimeout();
        long now = TimeUtils.currentTimeMillis();
        if (now - when < 0 || now - when > notificationTimeout) {
            return;
        }

        NotificationWrapperBuilder notificationWrapperBuilder =
                createNotificationBuilder()
                        .setSilent(true)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setAutoCancel(true)
                        .setLocalOnly(true)
                        .setContentTitle(title)
                        .setContentText(contents)
                        .setContentIntent(settingsIntentProvider)
                        .setWhen(when)
                        .setTimeoutAfter(notificationTimeout)
                        .addAction(
                                /* icon= */ 0,
                                review,
                                settingsIntentProvider,
                                NotificationUmaTracker.ActionType
                                        .SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS_REVIEW)
                        .addAction(
                                /* icon= */ 0,
                                ack,
                                ackIntentProvider,
                                NotificationUmaTracker.ActionType
                                        .SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS_ACK);

        NotificationWrapper notification =
                notificationWrapperBuilder.buildWithBigTextStyle(contents);

        BaseNotificationManagerProxyFactory.create().notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType
                                .SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS,
                        notification.getNotification());
    }

    /**
     * Searches a notifications list for the Safety Hub notification. Returns the notification, or
     * null if the notification is not found.
     */
    private static @Nullable Notification findNotification(
            List<? extends StatusBarNotificationProxy> notifications) {
        for (StatusBarNotificationProxy proxy : notifications) {
            if (proxy.getId()
                            == NotificationConstants
                                    .NOTIFICATION_ID_SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS
                    && proxy.getTag().equals(TAG)) {
                return proxy.getNotification();
            }
        }
        return null;
    }

    /**
     * Updates the notification, only if it is currently displayed, with the updated number of
     * revoked permissions. Everything else stays the same.
     *
     * @param numRevokedPermissions is the number of permissions revoked. If 0, the notification is
     *     dismissed.
     */
    @CalledByNative
    static void updateNotification(@JniType("int32_t") int numRevokedPermissions) {
        assert numRevokedPermissions >= 0
                : "This function expects a non-negative parameter numRevokedPermissions";
        if (numRevokedPermissions <= 0) {
            dismissNotification();
            return;
        }

        BaseNotificationManagerProxyFactory.create()
                .getActiveNotifications(
                        (activeNotifications) -> {
                            Notification activeNotification = findNotification(activeNotifications);
                            if (activeNotification == null) {
                                return;
                            }
                            displayOrUpdateNotification(
                                    numRevokedPermissions, activeNotification.when);
                        });
    }

    private static void dismissNotification() {
        BaseNotificationManagerProxyFactory.create()
                .cancel(
                        TAG,
                        NotificationConstants
                                .NOTIFICATION_ID_SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS);
    }
}
