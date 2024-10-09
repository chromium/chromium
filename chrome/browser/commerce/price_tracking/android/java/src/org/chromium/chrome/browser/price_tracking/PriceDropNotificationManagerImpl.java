// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

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
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;

import java.util.Locale;

/** Manage price drop notifications. */
public class PriceDropNotificationManagerImpl implements PriceDropNotificationManager {
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
    static final String EXTRA_PRODUCT_CLUSTER_ID =
            "org.chromium.chrome.browser.price_tracking.PRODUCT_CLUSTER_ID";
    static final String EXTRA_NOTIFICATION_ID =
            "org.chromium.chrome.browser.price_tracking.NOTIFICATION_ID";

    static final String CHROME_MANAGED_TIMESTAMPS =
            ChromePreferenceKeys.PRICE_TRACKING_CHROME_MANAGED_NOTIFICATIONS_TIMESTAMPS;
    static final String USER_MANAGED_TIMESTAMPS =
            ChromePreferenceKeys.PRICE_TRACKING_USER_MANAGED_NOTIFICATIONS_TIMESTAMPS;

    @VisibleForTesting
    public static final String NOTIFICATION_ENABLED_HISTOGRAM =
            "Commerce.PriceDrop.SystemNotificationEnabled";

    @VisibleForTesting
    public static final String NOTIFICATION_CHROME_MANAGED_COUNT_HISTOGRAM =
            "Commerce.PriceDrops.ChromeManaged.NotificationCount";

    @VisibleForTesting
    public static final String NOTIFICATION_USER_MANAGED_COUNT_HISTOGRAM =
            "Commerce.PriceDrops.UserManaged.NotificationCount";

    private static NotificationManagerProxy sNotificationManagerForTesting;

    /** Used to host click logic for "turn off alert" action intent. */
    public static class TrampolineActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            Intent intent = getIntent();
            String destinationUrl = IntentUtils.safeGetStringExtra(intent, EXTRA_DESTINATION_URL);
            String actionId = IntentUtils.safeGetStringExtra(intent, EXTRA_ACTION_ID);
            String offerId = IntentUtils.safeGetStringExtra(intent, EXTRA_OFFER_ID);
            String clusterId = IntentUtils.safeGetStringExtra(intent, EXTRA_PRODUCT_CLUSTER_ID);
            int notificationId = IntentUtils.safeGetIntExtra(intent, EXTRA_NOTIFICATION_ID, 0);

            dismissNotification(notificationId);

            if (TextUtils.isEmpty(offerId)) {
                Log.e(TAG, "No offer id is provided when handling turn off alert action.");
                finish();
                return;
            }

            // Handles "turn off alert" action button click.
            ChromeBrowserInitializer.getInstance()
                    .runNowOrAfterFullBrowserStarted(
                            () -> {
                                // TODO(339295368): Pass the Profile reference in the notification
                                // message details, and load the correct Profile here.
                                PriceDropNotificationManager priceDropNotificationManager =
                                        PriceDropNotificationManagerFactory.create(
                                                ProfileManager.getLastUsedRegularProfile());
                                assert ACTION_ID_TURN_OFF_ALERT.equals(actionId)
                                        : "Currently, only turn off alert action uses this.";
                                priceDropNotificationManager.onNotificationActionClicked(
                                        actionId,
                                        destinationUrl,
                                        offerId,
                                        clusterId,
                                        /* recordMetrics= */ false);
                                // Finish immediately. Could be better to have a callback from
                                // shopping backend.
                                finish();
                            });
        }
    }

    /** Used to dismiss the notification after content click or "visit site" action click. */
    public static class DismissNotificationChromeActivity extends ChromeLauncherActivity {
        @Override
        public void onCreate(@Nullable Bundle savedInstanceState) {
            int notificationId = IntentUtils.safeGetIntExtra(getIntent(), EXTRA_NOTIFICATION_ID, 0);
            dismissNotification(notificationId);
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    private final Context mContext;
    private final Profile mProfile;
    private final NotificationManagerProxy mNotificationManager;
    private final SharedPreferencesManager mPreferencesManager;

    /**
     * Constructor.
     *
     * @param context The application context.
     * @param profile The {@link Profile} associated with the price drops.
     * @param notificationManagerProxy The {@link NotificationManagerProxy} for sending
     *     notifications.
     */
    public PriceDropNotificationManagerImpl(
            Context context, Profile profile, NotificationManagerProxy notificationManagerProxy) {
        mContext = context;
        mProfile = profile;
        mNotificationManager = notificationManagerProxy;
        mPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @Override
    public boolean isEnabled() {
        return true;
    }

    @Override
    public boolean canPostNotification() {
        // Currently we only post notifications for explicit price tracking which is gated by the
        // "shopping list" feature flag. When we start implicit price tracking, we should use a
        // separate flag and add the check on it here.
        if (!areAppNotificationsEnabled()
                || !CommerceFeatureUtils.isShoppingListEligible(
                        ShoppingServiceFactory.getForProfile(mProfile))) {
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

    @Override
    public boolean canPostNotificationWithMetricsRecorded() {
        if (!CommerceFeatureUtils.isShoppingListEligible(
                ShoppingServiceFactory.getForProfile(mProfile))) {
            return false;
        }
        boolean isSystemNotificationEnabled = areAppNotificationsEnabled();
        RecordHistogram.recordBooleanHistogram(
                NOTIFICATION_ENABLED_HISTOGRAM, isSystemNotificationEnabled);
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

    @Override
    public void onNotificationPosted(@Nullable Notification notification) {
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                        notification);
    }

    @Override
    public void onNotificationClicked(String url) {
        NotificationUmaTracker.getInstance()
                .onNotificationContentClick(
                        NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                        NotificationIntentInterceptor.INVALID_CREATE_TIME);
    }

    @Override
    public void onNotificationActionClicked(
            String actionId, String url, @Nullable String offerId, boolean recordMetrics) {
        onNotificationActionClicked(actionId, url, offerId, null, recordMetrics);
    }

    @Override
    public void onNotificationActionClicked(
            String actionId,
            String url,
            @Nullable String offerId,
            @Nullable String clusterId,
            boolean recordMetrics) {
        if (actionId.equals(ACTION_ID_VISIT_SITE) && recordMetrics) {
            NotificationUmaTracker.getInstance()
                    .onNotificationActionClick(
                            NotificationUmaTracker.ActionType.PRICE_DROP_VISIT_SITE,
                            NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                            NotificationIntentInterceptor.INVALID_CREATE_TIME);
        } else if (actionId.equals(ACTION_ID_TURN_OFF_ALERT)) {
            if (offerId == null && clusterId == null) return;
            ShoppingService shoppingService = ShoppingServiceFactory.getForProfile(mProfile);
            Callback<Boolean> callback =
                    (status) -> {
                        assert status : "Failed to remove subscriptions.";
                        Log.e(TAG, "Failed to remove subscriptions.");
                    };
            final BookmarkModel bookmarkModel = BookmarkModel.getForProfile(mProfile);

            Runnable unsubscribeRunnable =
                    () -> {
                        if (offerId != null) {
                            shoppingService.unsubscribe(
                                    new CommerceSubscription(
                                            SubscriptionType.PRICE_TRACK,
                                            IdentifierType.OFFER_ID,
                                            offerId,
                                            ManagementType.CHROME_MANAGED,
                                            null),
                                    callback);
                        }
                        if (clusterId != null) {
                            shoppingService.unsubscribe(
                                    new CommerceSubscription(
                                            SubscriptionType.PRICE_TRACK,
                                            IdentifierType.PRODUCT_CLUSTER_ID,
                                            clusterId,
                                            ManagementType.USER_MANAGED,
                                            null),
                                    callback);
                        }
                    };

            // Only attempt to unsubscribe once the corresponding bookmarks can also be updated.
            if (bookmarkModel.isBookmarkModelLoaded()) {
                unsubscribeRunnable.run();
            } else {
                bookmarkModel.addObserver(
                        new BookmarkModelObserver() {
                            @Override
                            public void bookmarkModelLoaded() {
                                unsubscribeRunnable.run();
                                bookmarkModel.removeObserver(this);
                            }

                            @Override
                            public void bookmarkModelChanged() {}
                        });
            }

            if (recordMetrics) {
                NotificationUmaTracker.getInstance()
                        .onNotificationActionClick(
                                NotificationUmaTracker.ActionType.PRICE_DROP_TURN_OFF_ALERT,
                                NotificationUmaTracker.SystemNotificationType.PRICE_DROP_ALERTS,
                                NotificationIntentInterceptor.INVALID_CREATE_TIME);
            }
        }
    }

    @Override
    @Deprecated
    public Intent getNotificationClickIntent(String url) {
        return getNotificationClickIntent(url, 0);
    }

    @Override
    public Intent getNotificationClickIntent(String url, int notificationId) {
        // TODO(339295368): Pass a Profile reference so the correct activity is reopened based on
        // this notification being clicked.
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(url))
                        .setClass(mContext, DismissNotificationChromeActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
                        .putExtra(EXTRA_NOTIFICATION_ID, notificationId);
        IntentUtils.addTrustedIntentExtras(intent);
        return intent;
    }

    @Override
    @Deprecated
    public Intent getNotificationActionClickIntent(String actionId, String url, String offerId) {
        return getNotificationActionClickIntent(actionId, url, offerId, null, 0);
    }

    @Override
    @Deprecated
    public Intent getNotificationActionClickIntent(
            String actionId, String url, String offerId, String clusterId) {
        return getNotificationActionClickIntent(actionId, url, offerId, clusterId, 0);
    }

    @Override
    public Intent getNotificationActionClickIntent(
            String actionId, String url, String offerId, String clusterId, int notificationId) {
        if (ACTION_ID_VISIT_SITE.equals(actionId)) {
            return getNotificationClickIntent(url, notificationId);
        }
        if (ACTION_ID_TURN_OFF_ALERT.equals(actionId)) {
            Intent intent = new Intent(mContext, TrampolineActivity.class);
            intent.putExtra(EXTRA_DESTINATION_URL, url);
            intent.putExtra(EXTRA_ACTION_ID, actionId);
            intent.putExtra(EXTRA_OFFER_ID, offerId);
            if (clusterId != null) intent.putExtra(EXTRA_PRODUCT_CLUSTER_ID, clusterId);
            intent.putExtra(EXTRA_NOTIFICATION_ID, notificationId);
            IntentUtils.addTrustedIntentExtras(intent);
            return intent;
        }
        return null;
    }

    @Override
    public boolean areAppNotificationsEnabled() {
        if (sNotificationManagerForTesting != null) {
            return sNotificationManagerForTesting.areNotificationsEnabled();
        }
        return mNotificationManager.areNotificationsEnabled();
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.O)
    public void createNotificationChannel() {
        NotificationChannel channel = getNotificationChannel();
        if (channel != null) return;
        new ChannelsInitializer(
                        mNotificationManager,
                        ChromeChannelDefinitions.getInstance(),
                        mContext.getResources())
                .ensureInitialized(ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT);
    }

    @Override
    public void launchNotificationSettings() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // Make sure the channel is initialized before sending users to the settings.
            createNotificationChannel();
        }
        mContext.startActivity(getNotificationSettingsIntent());
    }

    @Override
    @VisibleForTesting
    public Intent getNotificationSettingsIntent() {
        Intent intent = new Intent();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (areAppNotificationsEnabled()) {
                intent.setAction(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE, mContext.getPackageName());
                intent.putExtra(
                        Settings.EXTRA_CHANNEL_ID,
                        ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT);
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

    @Override
    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.O)
    public NotificationChannel getNotificationChannel() {
        return mNotificationManager.getNotificationChannel(
                ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT);
    }

    /**
     * Set notificationManager for testing.
     *
     * @param notificationManager that will be set.
     */
    public static void setNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        sNotificationManagerForTesting = notificationManager;
        ResettersForTesting.register(() -> sNotificationManagerForTesting = null);
    }

    /** Delete price drop notification channel for testing. */
    @Override
    @RequiresApi(Build.VERSION_CODES.O)
    public void deleteChannelForTesting() {
        mNotificationManager.deleteNotificationChannel(
                ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT);
    }

    @Override
    public void recordMetricsForNotificationCounts() {
        RecordHistogram.recordCount100Histogram(
                NOTIFICATION_CHROME_MANAGED_COUNT_HISTOGRAM,
                updateNotificationTimestamps(
                        SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED, false));
        RecordHistogram.recordCount100Histogram(
                NOTIFICATION_USER_MANAGED_COUNT_HISTOGRAM,
                updateNotificationTimestamps(
                        SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED, false));
    }

    @Override
    public boolean hasReachedMaxAllowedNotificationNumber(@SystemNotificationType int type) {
        boolean hasReached =
                updateNotificationTimestamps(type, false)
                        >= PriceTrackingNotificationConfig.getMaxAllowedNotificationNumber(type);
        String managementType = notificationTypeToManagementType(type);
        if (managementType != null) {
            RecordHistogram.recordBooleanHistogram(
                    String.format(
                            Locale.US,
                            "Commerce.PriceDrops.%s.NotificationReachedCap",
                            managementType),
                    hasReached);
        }
        return hasReached;
    }

    @Override
    public int updateNotificationTimestamps(
            @SystemNotificationType int type, boolean attachCurrentTime) {
        long currentTime = System.currentTimeMillis();
        JSONArray newTimestamps = new JSONArray();
        try {
            String oldSerializedTimestamps = getStoredNotificationTimestamps(type);
            JSONArray oldTimestamps = new JSONArray(oldSerializedTimestamps);
            for (int i = 0; i < oldTimestamps.length(); i++) {
                long timestamp = oldTimestamps.getLong(i);
                if (currentTime - timestamp
                        > PriceTrackingNotificationConfig
                                .getNotificationTimestampsStoreWindowMs()) {
                    continue;
                }
                newTimestamps.put(timestamp);
            }
        } catch (JSONException e) {
            Log.e(
                    TAG,
                    String.format(
                            Locale.US,
                            "Failed to parse notification timestamps. Details: %s",
                            e.getMessage()));
            // If one parse fails, we discard all data and reset the stored timestamps.
            newTimestamps = new JSONArray();
        }
        if (attachCurrentTime) newTimestamps.put(currentTime);
        writeSerializedNotificationTimestamps(type, newTimestamps.toString());
        return newTimestamps.length();
    }

    private String getStoredNotificationTimestamps(@SystemNotificationType int type) {
        String serializedTimestamps = "";
        if (type == SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED) {
            serializedTimestamps = mPreferencesManager.readString(CHROME_MANAGED_TIMESTAMPS, "");
        } else if (type == SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED) {
            serializedTimestamps = mPreferencesManager.readString(USER_MANAGED_TIMESTAMPS, "");
        }
        return serializedTimestamps;
    }

    private void writeSerializedNotificationTimestamps(
            @SystemNotificationType int type, String serializedTimestamps) {
        if (type == SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED) {
            mPreferencesManager.writeString(CHROME_MANAGED_TIMESTAMPS, serializedTimestamps);
        } else if (type == SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED) {
            mPreferencesManager.writeString(USER_MANAGED_TIMESTAMPS, serializedTimestamps);
        }
    }

    private String notificationTypeToManagementType(@SystemNotificationType int type) {
        if (type == SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED) {
            return "ChromeManaged";
        } else if (type == SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED) {
            return "UserManaged";
        } else {
            Log.e(TAG, "Invalid notification type.");
            return null;
        }
    }

    private static void dismissNotification(int notificationId) {
        new NotificationManagerProxyImpl(ContextUtils.getApplicationContext())
                .cancel(PriceDropNotifier.NOTIFICATION_TAG, notificationId);
    }
}
