// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.app.Notification;
import android.app.PendingIntent;
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
    private static final String TAG = "safety_hub";

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

    private static boolean isAutoRevokeSuspiciousNotificationEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTO_REVOKE_SUSPICIOUS_NOTIFICATION);
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
     * @param firstAffectedDomain is the domain of the first affected permission.
     * @param anySuspiciousRevocations is true if any of the revoked permissions were due to
     *     suspicious content.
     * @param anyDisruptiveRevocations is true if any of the revoked permissions were due to
     *     disruptive content.
     */
    @CalledByNative
    static void displayNotification(
            @JniType("int32_t") int numRevokedPermissions,
            @JniType("std::string") String firstAffectedDomain,
            @JniType("bool") boolean anySuspiciousRevocations,
            @JniType("bool") boolean anyDisruptiveRevocations) {
        assert numRevokedPermissions >= 0
                : "This function expects a non-negative parameter numRevokedPermissions";
        if (numRevokedPermissions <= 0) {
            dismissNotification();
            return;
        }
        displayOrUpdateNotification(
                numRevokedPermissions,
                TimeUtils.currentTimeMillis(),
                firstAffectedDomain,
                anySuspiciousRevocations,
                anyDisruptiveRevocations);
    }

    private static void displayOrUpdateNotification(
            int numRevokedPermissions,
            long when,
            String firstAffectedDomain,
            boolean anySuspiciousRevocations,
            boolean anyDisruptiveRevocations) {
        if (!isDisruptiveNotificationRevocationEnabled()
                && !isAutoRevokeSuspiciousNotificationEnabled()) {
            dismissNotification();
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        Resources res = context.getResources();
        String title = getNotificationTitle(numRevokedPermissions, firstAffectedDomain, res);
        String contents =
                getNotificationContents(
                        numRevokedPermissions,
                        anySuspiciousRevocations,
                        anyDisruptiveRevocations,
                        res);
        if (contents == null) {
            dismissNotification();
            return;
        }

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent settingsIntent =
                settingsNavigation.createSettingsIntent(
                        context, SafetyHubPermissionsFragment.class);

        PendingIntentProvider settingsIntentProvider =
                PendingIntentProvider.getActivity(
                        context, 0, settingsIntent, PendingIntent.FLAG_UPDATE_CURRENT);

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
                                        .SAFETY_HUB_UNSUBSCRIBED_NOTIFICATIONS_REVIEW);

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
     * Helper method to get the notification title based on the number of revoked permissions and
     * the first affected domain.
     */
    private static String getNotificationTitle(
            int numRevokedPermissions, String firstAffectedDomain, Resources res) {
        if (numRevokedPermissions == 1) {
            return res.getString(
                    R.string
                            .safety_hub_unsubscribed_disruptive_and_suspicious_notifications_notification_title_singular,
                    firstAffectedDomain);
        }
        return res.getString(
                R.string
                        .safety_hub_unsubscribed_disruptive_and_suspicious_notifications_notification_title_plural,
                numRevokedPermissions);
    }

    /**
     * Helper method to get the notification contents based on the number of revoked permissions and
     * the type of revocations.
     */
    private static @Nullable String getNotificationContents(
            int numRevokedPermissions,
            boolean anySuspiciousRevocations,
            boolean anyDisruptiveRevocations,
            Resources res) {
        if (anySuspiciousRevocations && anyDisruptiveRevocations) {
            return res.getString(
                    R.string
                            .safety_hub_unsubscribed_disruptive_and_suspicious_notifications_notification_message);
        } else if (anySuspiciousRevocations) {
            return res.getQuantityString(
                    R.plurals.safety_hub_unsubscribed_suspicious_notifications_notification_message,
                    numRevokedPermissions);
        } else if (anyDisruptiveRevocations) {
            return res.getQuantityString(
                    R.plurals.safety_hub_unsubscribed_disruptive_notifications_notification_message,
                    numRevokedPermissions);
        }
        return null;
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
     * @param firstAffectedDomain is the domain of the first affected permission.
     * @param anySuspiciousRevocations is true if any of the revoked permissions were due to
     *     suspicious content.
     * @param anyDisruptiveRevocations is true if any of the revoked permissions were due to
     *     disruptive content.
     */
    @CalledByNative
    static void updateNotification(
            @JniType("int32_t") int numRevokedPermissions,
            @JniType("std::string") String firstAffectedDomain,
            @JniType("bool") boolean anySuspiciousRevocations,
            @JniType("bool") boolean anyDisruptiveRevocations) {
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
                                    numRevokedPermissions,
                                    activeNotification.when,
                                    firstAffectedDomain,
                                    anySuspiciousRevocations,
                                    anyDisruptiveRevocations);
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
