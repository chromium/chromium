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
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.provider.Settings;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManagerImpl;
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
    // The action ids should be the same as defined in the server, see {@link
    // HandleProductUpdateEventsProducerModule}.
    private static final String ACTION_ID_VISIT_SITE = "visit_site";
    private static final String ACTION_ID_TURN_OFF_ALERT = "turn_off_alert";

    private static NotificationManagerProxy sNotificationManagerForTesting;

    private final Context mContext;
    private final NotificationManagerProxy mNotificationManager;

    public PriceDropNotificationManager() {
        mContext = ContextUtils.getApplicationContext();
        mNotificationManager = new NotificationManagerProxyImpl(mContext);
    }

    /**
     * @return Whether the price drop notification type is enabled. For now it is used in downstream
     *         which could influence the Chime registration.
     */
    public boolean isEnabled() {
        return PriceTrackingUtilities.ENABLE_PRICE_NOTIFICATION.getValue();
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
     * When user clicks the notification, they will be sent to the tab with price drop which
     * triggered the notification.
     *
     * @param url of the tab which triggered the notification.
     */
    public void onNotificationClicked(String url) {
        mContext.startActivity(getNotificationClickIntent(url));
        NotificationUmaTracker.getInstance().onNotificationContentClick(
                NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                NotificationIntentInterceptor.INVALID_CREATE_TIME);
    }

    /**
     * Handles the notification action click events.
     *
     * @param actionId the id used to identify certain action.
     * @param url of the tab which triggered the notification.
     * @param offerId the id of the offer associated with this notification.
     */
    public void onNotificationActionClicked(String actionId, String url, @Nullable String offerId) {
        if (actionId.equals(ACTION_ID_VISIT_SITE)) {
            mContext.startActivity(getNotificationClickIntent(url));
            NotificationUmaTracker.getInstance().onNotificationActionClick(
                    NotificationUmaTracker.ActionType.PRICE_DROP_VISIT_SITE,
                    NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                    NotificationIntentInterceptor.INVALID_CREATE_TIME);
        } else if (actionId.equals(ACTION_ID_TURN_OFF_ALERT)) {
            if (offerId == null) return;
            SubscriptionsManagerImpl subscriptionsManager =
                    (new CommerceSubscriptionsServiceFactory())
                            .getForLastUsedProfile()
                            .getSubscriptionsManager();
            subscriptionsManager.unsubscribe(
                    new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK, offerId,
                            SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID),
                    (didSucceed) -> { assert didSucceed : "Failed to remove subscriptions."; });
            NotificationUmaTracker.getInstance().onNotificationActionClick(
                    NotificationUmaTracker.ActionType.PRICE_DROP_TURN_OFF_ALERT,
                    NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                    NotificationIntentInterceptor.INVALID_CREATE_TIME);
        }
    }

    /**
     * @return The intent that we will use to send users to the tab which triggered the
     *         notification.
     *
     * @param url of the tab which triggered the notification.
     */
    @VisibleForTesting
    public Intent getNotificationClickIntent(String url) {
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(url))
                        .setClass(mContext, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName())
                        .putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent);
        return intent;
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
