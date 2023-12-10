// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.app.Notification;
import android.app.NotificationChannel;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;

/** Manage price drop notifications. */
public interface PriceDropNotificationManager {
    /**
     * @return Whether the price drop notification type is enabled. For now it is used in downstream
     *         which could influence the Chime registration.
     */
    boolean isEnabled();

    /**
     * @return Whether price drop notifications can be posted.
     */
    boolean canPostNotification();

    /**
     * @return Whether price drop notifications can be posted and record user opt-in metrics.
     */
    boolean canPostNotificationWithMetricsRecorded();

    /**
     * Record UMAs after posting price drop notifications.
     *
     * @param notification that has been posted.
     */
    void onNotificationPosted(@Nullable Notification notification);

    /**
     * When user clicks the notification, they will be sent to the tab with price drop which
     * triggered the notification. Only Chime notification code path should use this.
     *
     * @param url of the tab which triggered the notification.
     */
    void onNotificationClicked(String url);

    /**
     * Handles the notification action click events.
     *
     * @param actionId the id used to identify certain action.
     * @param url of the tab which triggered the notification.
     * @param offerId the id of the offer associated with this notification.
     * @param recordMetrics Whether to record metrics using {@link NotificationUmaTracker}. Only
     *         Chime notification code path should set this to true.
     */
    void onNotificationActionClicked(
            String actionId, String url, @Nullable String offerId, boolean recordMetrics);

    /**
     * Handles the notification action click events.
     *
     * @param actionId the id used to identify certain action.
     * @param url of the tab which triggered the notification.
     * @param offerId the id of the offer associated with this notification.
     * @param clusterId The id of the cluster associated with the product notification.
     * @param recordMetrics Whether to record metrics using {@link NotificationUmaTracker}. Only
     *         Chime notification code path should set this to true.
     */
    void onNotificationActionClicked(
            String actionId,
            String url,
            @Nullable String offerId,
            @Nullable String clusterId,
            boolean recordMetrics);

    @Deprecated
    Intent getNotificationClickIntent(String url);

    /**
     * @return The intent that we will use to send users to the tab which triggered the
     *         notification.
     *
     * @param url of the tab which triggered the notification.
     * @param notificationId the notification id.
     */
    Intent getNotificationClickIntent(String url, int notificationId);

    @Deprecated
    Intent getNotificationActionClickIntent(String actionId, String url, String offerId);

    @Deprecated
    Intent getNotificationActionClickIntent(
            String actionId, String url, String offerId, String clusterId);

    /**
     * Gets the notification action click intents.
     *
     * @param actionId the id used to identify certain action.
     * @param url of the tab which triggered the notification.
     * @param offerId The offer id of the product.
     * @param clusterId The cluster id of the product.
     * @param notificationId the notification id.
     */
    Intent getNotificationActionClickIntent(
            String actionId, String url, String offerId, String clusterId, int notificationId);

    /**
     * @return Whether app notifications are enabled.
     */
    boolean areAppNotificationsEnabled();

    /** Create the notification channel for price drop notifications. */
    @RequiresApi(Build.VERSION_CODES.O)
    void createNotificationChannel();

    /** Send users to notification settings so they can manage price drop notifications. */
    void launchNotificationSettings();

    /**
     * @return The intent that we will use to send users to notification settings.
     */
    @VisibleForTesting
    Intent getNotificationSettingsIntent();

    /**
     * @return The price drop notification channel.
     */
    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.O)
    NotificationChannel getNotificationChannel();

    /** Delete price drop notification channel for testing. */
    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.O)
    void deleteChannelForTesting();

    /** Record how many notifications are shown in the given window per management type. */
    void recordMetricsForNotificationCounts();

    /** Check if the shown notifications in given window have reached the max allowed number. */
    boolean hasReachedMaxAllowedNotificationNumber(@SystemNotificationType int type);

    /**
     * Update the stored notification timestamps. Outdated timestamps are removed and current
     * timestamp could be attached.
     *
     * @param type The notification UMA type.
     * @param attachCurrentTime Whether to store current timestamp.
     * @return the number of stored timestamps after update.
     */
    int updateNotificationTimestamps(@SystemNotificationType int type, boolean attachCurrentTime);
}
