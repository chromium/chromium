// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/**
 * Manage price drop notifications.
 */
public class PriceDropNotificationManager {
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";
    private static final String EXTRA_APP_PACKAGE = "app_package";
    private static final String EXTRA_APP_UID = "app_uid";

    private static NotificationManagerProxy sNotificationManagerForTesting;

    private final Context mContext;
    private final NotificationManagerProxy mNotificationManager;

    public PriceDropNotificationManager() {
        mContext = ContextUtils.getApplicationContext();
        mNotificationManager = new NotificationManagerProxyImpl(mContext);
    }

    /**
     * @return Whether price drop notifications can be posted.
     */
    public boolean canPostNotification() {
        if (!areAppNotificationsEnabled()
                || !PriceTrackingUtilities.isPriceDropNotificationEligible()) {
            return false;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = getNotificationChannel();
            if (channel == null || channel.getImportance() == NotificationManager.IMPORTANCE_NONE) {
                return false;
            }
        }

        return true;
    }

    /**
     * Record UMAs after posting price drop notifications.
     *
     * @param notification that has been posted.
     */
    public void onNotificationPosted(@Nullable Notification notification) {
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS, notification);
    }

    /**
     * @return Whether app notifications are enabled.
     */
    public boolean areAppNotificationsEnabled() {
        if (sNotificationManagerForTesting != null) {
            return sNotificationManagerForTesting.areNotificationsEnabled();
        }
        return mNotificationManager.areNotificationsEnabled();
    }

    /**
     * Create the notification channel for price drop notifications.
     */
    @TargetApi(Build.VERSION_CODES.O)
    public void createNotificationChannel() {
        NotificationChannel channel = getNotificationChannel();
        if (channel != null) return;
        new ChannelsInitializer(mNotificationManager, ChromeChannelDefinitions.getInstance(),
                mContext.getResources())
                .ensureInitialized(ChromeChannelDefinitions.ChannelId.PRICE_DROP);
    }

    /**
     * Send users to notification settings so they can manage price drop notifications.
     */
    public void launchNotificationSettings() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Make sure the channel is initialized before sending users to the settings.
            createNotificationChannel();
        }
        mContext.startActivity(getNotificationSettingsIntent());
        // Disable PriceAlertsMessageCard after the first time we send users to notification
        // settings.
        PriceTrackingUtilities.disablePriceAlertsMessageCard();
    }

    /**
     * @return The intent that we will use to send users to notification settings.
     */
    @VisibleForTesting
    public Intent getNotificationSettingsIntent() {
        Intent intent = new Intent();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (areAppNotificationsEnabled()) {
                intent.setAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName());
                intent.putExtra(
                        Settings.EXTRA_CHANNEL_ID, ChromeChannelDefinitions.ChannelId.PRICE_DROP);
            } else {
                intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName());
            }
        } else {
            intent.setAction(ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(EXTRA_APP_PACKAGE, mContext.getPackageName());
            intent.putExtra(EXTRA_APP_UID, mContext.getApplicationInfo().uid);
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /**
     * @return The price drop notification channel.
     */
    @VisibleForTesting
    @TargetApi(Build.VERSION_CODES.O)
    public NotificationChannel getNotificationChannel() {
        return mNotificationManager.getNotificationChannel(
                ChromeChannelDefinitions.ChannelId.PRICE_DROP);
    }

    /**
     * Set notificationManager for testing.
     *
     * @param notificationManager that will be set.
     */
    @VisibleForTesting
    public static void setNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        sNotificationManagerForTesting = notificationManager;
    }

    /**
     * Delete price drop notification channel for testing.
     */
    @VisibleForTesting
    @TargetApi(Build.VERSION_CODES.O)
    public void deleteChannelForTesting() {
        mNotificationManager.deleteNotificationChannel(
                ChromeChannelDefinitions.ChannelId.PRICE_DROP);
    }
}
