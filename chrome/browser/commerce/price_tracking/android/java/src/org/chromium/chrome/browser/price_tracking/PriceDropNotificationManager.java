// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceFactory;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManagerImpl;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

import java.util.Locale;
/**
 * Manage price drop notifications.
 */
public class PriceDropNotificationManager {
    private static final String TAG = "PriceDropNotif";
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";
    private static final String EXTRA_APP_PACKAGE = "app_package";
    private static final String EXTRA_APP_UID = "app_uid";
    // The action ids should be the same as defined in the server, see {@link
    // HandleProductUpdateEventsProducerModule}.
    static final String ACTION_ID_VISIT_SITE = "visit_site";
    static final String ACTION_ID_TURN_OFF_ALERT = "turn_off_alert";

    static final String EXTRA_DESTINATION_URL =
            "org.chromium.chrome.browser.price_tracking.DESTINATION_URL";
    static final String EXTRA_ACTION_ID = "org.chromium.chrome.browser.price_tracking.ACTION_ID";
    static final String EXTRA_OFFER_ID = "org.chromium.chrome.browser.price_tracking.OFFER_ID";

    private static NotificationManagerProxy sNotificationManagerForTesting;

    /**
     * Used to host click logic for "turn off alert" action intent.
     */
    public static class TrampolineActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Intent intent = getIntent();
            String destinationUrl = IntentUtils.safeGetStringExtra(intent, EXTRA_DESTINATION_URL);
            String actionId = IntentUtils.safeGetStringExtra(intent, EXTRA_ACTION_ID);
            String offerId = IntentUtils.safeGetStringExtra(intent, EXTRA_OFFER_ID);

            if (TextUtils.isEmpty(offerId)) {
                Log.e(TAG, "No offer id is provided when handling turn off alert action.");
                finish();
                return;
            }

            // Handles "turn off alert" action button click.
            ChromeBrowserInitializer.getInstance().runNowOrAfterFullBrowserStarted(() -> {
                PriceDropNotificationManager priceDropNotificationManager =
                        new PriceDropNotificationManager();
                assert ACTION_ID_TURN_OFF_ALERT.equals(actionId)
                    : "Currently only turn off alert action uses this activity.";
                priceDropNotificationManager.onNotificationActionClicked(
                        actionId, destinationUrl, offerId, /*recordMetrics=*/false);
                // Finish immediately. Could be better to have a callback from shopping backend.
                finish();
            });
        }
    }

    private final Context mContext;
    private final NotificationManagerProxy mNotificationManager;

    public PriceDropNotificationManager() {
        this(ContextUtils.getApplicationContext(),
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext()));
    }

    public PriceDropNotificationManager(
            Context context, NotificationManagerProxy notificationManagerProxy) {
        mContext = context;
        mNotificationManager = notificationManagerProxy;
    }

    /**
     * @return Whether the price drop notification type is enabled. For now it is used in downstream
     *         which could influence the Chime registration.
     */
    public boolean isEnabled() {
        return PriceTrackingUtilities.getPriceTrackingNotificationsEnabled();
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
     * @return Whether price drop notifications can be posted and record user opt-in metrics.
     */
    public boolean canPostNotificationWithMetricsRecorded() {
        if (!PriceTrackingUtilities.isPriceDropNotificationEligible()) return false;
        boolean isSystemNotificationEnabled = areAppNotificationsEnabled();
        RecordHistogram.recordBooleanHistogram(
                "Commerce.PriceDrop.SystemNotificationEnabled", isSystemNotificationEnabled);
        if (!isSystemNotificationEnabled) return false;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return true;

        NotificationChannel channel = getNotificationChannel();
        boolean isChannelCreated = channel != null;
        RecordHistogram.recordBooleanHistogram(
                "Commerce.PriceDrop.NotificationChannelCreated", isChannelCreated);
        if (!isChannelCreated) return false;
        boolean isChannelBlocked = channel.getImportance() == NotificationManager.IMPORTANCE_NONE;
        RecordHistogram.recordBooleanHistogram(
                "Commerce.PriceDrop.NotificationChannelBlocked", isChannelBlocked);
        return !isChannelBlocked;
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
     * triggered the notification. Only Chime notification code path should use this.
     *
     * @param url of the tab which triggered the notification.
     */
    public void onNotificationClicked(String url) {
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
     * @param recordMetrics Whether to record metrics using {@link NotificationUmaTracker}. Only
     *         Chime notification code path should set this to true.
     */
    public void onNotificationActionClicked(
            String actionId, String url, @Nullable String offerId, boolean recordMetrics) {
        if (actionId.equals(ACTION_ID_VISIT_SITE) && recordMetrics) {
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
                    (status) -> {
                        assert status
                                == SubscriptionsManager.StatusCode.OK
                            : "Failed to remove subscriptions.";
                        Log.e(TAG,
                                String.format(Locale.US,
                                        "Failed to remove subscriptions. Status: %d", status));
                    });
            if (recordMetrics) {
                NotificationUmaTracker.getInstance().onNotificationActionClick(
                        NotificationUmaTracker.ActionType.PRICE_DROP_TURN_OFF_ALERT,
                        NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                        NotificationIntentInterceptor.INVALID_CREATE_TIME);
            }
        }
    }

    /**
     * @return The intent that we will use to send users to the tab which triggered the
     *         notification.
     *
     * @param url of the tab which triggered the notification.
     */
    public Intent getNotificationClickIntent(String url) {
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(url))
                        .setClass(mContext, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }

    /**
     * Gets the notification action click intents.
     *
     * @param actionId the id used to identify certain action.
     * @param url of the tab which triggered the notification.
     * @param offerId The offer id of the product.
     */
    public Intent getNotificationActionClickIntent(String actionId, String url, String offerId) {
        if (ACTION_ID_VISIT_SITE.equals(actionId)) return getNotificationClickIntent(url);
        if (ACTION_ID_TURN_OFF_ALERT.equals(actionId)) {
            Intent intent = new Intent(mContext, TrampolineActivity.class);
            intent.putExtra(EXTRA_DESTINATION_URL, url);
            intent.putExtra(EXTRA_ACTION_ID, actionId);
            intent.putExtra(EXTRA_OFFER_ID, offerId);
            return intent;
        }
        return null;
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
