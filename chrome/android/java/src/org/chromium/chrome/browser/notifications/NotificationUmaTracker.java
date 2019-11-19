// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.v4.app.NotificationManagerCompat;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.util.MathUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to make tracking notification UMA stats easier for various features.  Having a
 * single entry point here to make more complex tracking easier to add in the future.
 */
public class NotificationUmaTracker {
    private static final String TAG = "NotifsUMATracker";

    /*
     * A list of notification types.  To add a type to this list please update
     * SystemNotificationType in enums.xml and make sure to keep this list in sync.  Additions
     * should be treated as APPEND ONLY to keep the UMA metric semantics the same over time.
     *
     * A SystemNotificationType value can also be saved in shared preferences.
     */
    @IntDef({SystemNotificationType.UNKNOWN, SystemNotificationType.DOWNLOAD_FILES,
            SystemNotificationType.DOWNLOAD_PAGES, SystemNotificationType.CLOSE_INCOGNITO,
            SystemNotificationType.CONTENT_SUGGESTION, SystemNotificationType.MEDIA_CAPTURE,
            SystemNotificationType.PHYSICAL_WEB, SystemNotificationType.MEDIA,
            SystemNotificationType.SITES, SystemNotificationType.SYNC,
            SystemNotificationType.WEBAPK, SystemNotificationType.BROWSER_ACTIONS,
            SystemNotificationType.WEBAPP_ACTIONS,
            SystemNotificationType.OFFLINE_CONTENT_SUGGESTION,
            SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES, SystemNotificationType.OFFLINE_PAGES,
            SystemNotificationType.SEND_TAB_TO_SELF, SystemNotificationType.UPDATES,
            SystemNotificationType.CLICK_TO_CALL, SystemNotificationType.SHARED_CLIPBOARD,
            SystemNotificationType.PERMISSION_REQUESTS,
            SystemNotificationType.PERMISSION_REQUESTS_HIGH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SystemNotificationType {
        int UNKNOWN = -1;
        int DOWNLOAD_FILES = 0;
        int DOWNLOAD_PAGES = 1;
        int CLOSE_INCOGNITO = 2;
        int CONTENT_SUGGESTION = 3;
        int MEDIA_CAPTURE = 4;
        int PHYSICAL_WEB = 5;
        int MEDIA = 6;
        int SITES = 7;
        int SYNC = 8;
        int WEBAPK = 9;
        int BROWSER_ACTIONS = 10;
        int WEBAPP_ACTIONS = 11;
        int OFFLINE_CONTENT_SUGGESTION = 12;
        int TRUSTED_WEB_ACTIVITY_SITES = 13;
        int OFFLINE_PAGES = 14;
        int SEND_TAB_TO_SELF = 15;
        int UPDATES = 16;
        int CLICK_TO_CALL = 17;
        int SHARED_CLIPBOARD = 18;
        int PERMISSION_REQUESTS = 19;
        int PERMISSION_REQUESTS_HIGH = 20;

        int NUM_ENTRIES = 21;
    }

    /*
     * A list of notification action types, each maps to a notification button.
     * To add a type to this list please update SystemNotificationActionType in enums.xml and make
     * sure to keep this list in sync.  Additions should be treated as APPEND ONLY to keep the UMA
     * metric semantics the same over time.
     */
    @IntDef({ActionType.UNKNOWN, ActionType.DOWNLOAD_PAUSE, ActionType.DOWNLOAD_RESUME,
            ActionType.DOWNLOAD_CANCEL, ActionType.DOWNLOAD_PAGE_PAUSE,
            ActionType.DOWNLOAD_PAGE_RESUME, ActionType.DOWNLOAD_PAGE_CANCEL,
            ActionType.CONTENT_SUGGESTION_SETTINGS, ActionType.WEB_APP_ACTION_SHARE,
            ActionType.WEB_APP_ACTION_OPEN_IN_CHROME,
            ActionType.OFFLINE_CONTENT_SUGGESTION_SETTINGS, ActionType.SHARING_TRY_AGAIN,
            ActionType.SETTINGS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionType {
        int UNKNOWN = -1;
        // Pause button on user download notification.
        int DOWNLOAD_PAUSE = 0;
        // Resume button on user download notification.
        int DOWNLOAD_RESUME = 1;
        // Cancel button on user download notification.
        int DOWNLOAD_CANCEL = 2;
        // Pause button on page download notification.
        int DOWNLOAD_PAGE_PAUSE = 3;
        // Resume button on page download notification.
        int DOWNLOAD_PAGE_RESUME = 4;
        // Cancel button on page download notification.
        int DOWNLOAD_PAGE_CANCEL = 5;
        // Setting button on content suggestion notification.
        int CONTENT_SUGGESTION_SETTINGS = 6;
        // Share button on web app action notification.
        int WEB_APP_ACTION_SHARE = 7;
        // Open in Chrome button on web app action notification.
        int WEB_APP_ACTION_OPEN_IN_CHROME = 8;
        // Setting button in offline content suggestion notification.
        int OFFLINE_CONTENT_SUGGESTION_SETTINGS = 9;
        // Dismiss button on sharing notification.
        // int SHARING_DISMISS = 10; deprecated
        // Try again button on sharing error notification.
        int SHARING_TRY_AGAIN = 11;
        // Settings button for notifications.
        int SETTINGS = 12;

        int NUM_ENTRIES = 13;
    }

    private static final String LAST_SHOWN_NOTIFICATION_TYPE_KEY =
            "NotificationUmaTracker.LastShownNotificationType";

    private static class LazyHolder {
        private static final NotificationUmaTracker INSTANCE = new NotificationUmaTracker();
    }

    /** Cached objects. */
    private final SharedPreferences mSharedPreferences;
    private final NotificationManagerCompat mNotificationManager;

    public static NotificationUmaTracker getInstance() {
        return LazyHolder.INSTANCE;
    }

    private NotificationUmaTracker() {
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
        mNotificationManager = NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    /**
     * Logs {@link android.app.Notification} usage, categorized into {@link SystemNotificationType}
     * types.  Splits the logs by the global enabled state of notifications and also logs the last
     * notification shown prior to the global notifications state being disabled by the user.
     * @param type The type of notification that was shown.
     * @param notification The notification that was shown.
     * @see SystemNotificationType
     */
    public void onNotificationShown(
            @SystemNotificationType int type, @Nullable Notification notification) {
        if (type == SystemNotificationType.UNKNOWN || notification == null) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            logNotificationShown(type, notification.getChannelId());
        } else {
            logNotificationShown(type, null);
        }
    }

    /**
     * Logs notification click event when the user taps on the notification body.
     * @param type Type of the notification.
     * @param createTime The notification creation timestamp.
     */
    public void onNotificationContentClick(@SystemNotificationType int type, long createTime) {
        if (type == SystemNotificationType.UNKNOWN) return;

        new CachedMetrics
                .EnumeratedHistogramSample("Mobile.SystemNotification.Content.Click",
                        SystemNotificationType.NUM_ENTRIES)
                .record(type);
        recordNotificationAgeHistogram("Mobile.SystemNotification.Content.Click.Age", createTime);

        switch (type) {
            case SystemNotificationType.SEND_TAB_TO_SELF:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.SendTabToSelf", createTime);
                break;
            case SystemNotificationType.CLICK_TO_CALL:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.ClickToCall", createTime);
                break;
            case SystemNotificationType.SHARED_CLIPBOARD:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Content.Click.Age.SharedClipboard", createTime);
                break;
        }
    }

    /**
     * Logs notification dismiss event the user swipes away the notification.
     * @param type Type of the notification.
     * @param createTime The notification creation timestamp.
     */
    public void onNotificationDismiss(@SystemNotificationType int type, long createTime) {
        if (type == SystemNotificationType.UNKNOWN) return;

        // TODO(xingliu): This may not work if Android kill Chrome before native library is loaded.
        // Cache data in Android shared preference and flush them to native when available.
        new CachedMetrics
                .EnumeratedHistogramSample(
                        "Mobile.SystemNotification.Dismiss", SystemNotificationType.NUM_ENTRIES)
                .record(type);
        recordNotificationAgeHistogram("Mobile.SystemNotification.Dismiss.Age", createTime);

        switch (type) {
            case SystemNotificationType.SEND_TAB_TO_SELF:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.SendTabToSelf", createTime);
                break;
            case SystemNotificationType.CLICK_TO_CALL:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.ClickToCall", createTime);
                break;
            case SystemNotificationType.SHARED_CLIPBOARD:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Dismiss.Age.SharedClipboard", createTime);
                break;
        }
    }

    /**
     * Logs notification button click event.
     * @param actionType Type of the notification action button.
     * @param notificationType Type of the notification.
     * @param createTime The notification creation timestamp.
     */
    public void onNotificationActionClick(@ActionType int actionType,
            @SystemNotificationType int notificationType, long createTime) {
        if (actionType == ActionType.UNKNOWN) return;

        // TODO(xingliu): This may not work if Android kill Chrome before native library is loaded.
        // Cache data in Android shared preference and flush them to native when available.
        new CachedMetrics
                .EnumeratedHistogramSample(
                        "Mobile.SystemNotification.Action.Click", ActionType.NUM_ENTRIES)
                .record(actionType);
        recordNotificationAgeHistogram("Mobile.SystemNotification.Action.Click.Age", createTime);

        switch (notificationType) {
            case SystemNotificationType.SEND_TAB_TO_SELF:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.SendTabToSelf", createTime);
                break;
            case SystemNotificationType.CLICK_TO_CALL:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.ClickToCall", createTime);
                break;
            case SystemNotificationType.SHARED_CLIPBOARD:
                recordNotificationAgeHistogram(
                        "Mobile.SystemNotification.Action.Click.Age.SharedClipboard", createTime);
                break;
        }
    }

    /**
     * Logs when failed to create notification with Android API.
     * @param type Type of the notification.
     */
    public static void onNotificationFailedToCreate(@SystemNotificationType int type) {
        if (type == SystemNotificationType.UNKNOWN) return;
        recordHistogram("Mobile.SystemNotification.CreationFailure", type);
    }

    private void logNotificationShown(
            @SystemNotificationType int type, @ChannelDefinitions.ChannelId String channelId) {
        if (!mNotificationManager.areNotificationsEnabled()) {
            logPotentialBlockedCause();
            recordHistogram("Mobile.SystemNotification.Blocked", type);
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && channelId != null
                && isChannelBlocked(channelId)) {
            recordHistogram("Mobile.SystemNotification.ChannelBlocked", type);
            return;
        }
        saveLastShownNotification(type);
        recordHistogram("Mobile.SystemNotification.Shown", type);
    }

    @TargetApi(26)
    private boolean isChannelBlocked(@ChannelDefinitions.ChannelId String channelId) {
        // Use non-compat notification manager as compat does not have getNotificationChannel (yet).
        NotificationManager notificationManager =
                ContextUtils.getApplicationContext().getSystemService(NotificationManager.class);
        NotificationChannel channel = notificationManager.getNotificationChannel(channelId);
        return channel != null && channel.getImportance() == NotificationManager.IMPORTANCE_NONE;
    }

    private void saveLastShownNotification(@SystemNotificationType int type) {
        mSharedPreferences.edit().putInt(LAST_SHOWN_NOTIFICATION_TYPE_KEY, type).apply();
    }

    private void logPotentialBlockedCause() {
        int lastType = mSharedPreferences.getInt(
                LAST_SHOWN_NOTIFICATION_TYPE_KEY, SystemNotificationType.UNKNOWN);
        if (lastType == -1) return;
        mSharedPreferences.edit().remove(LAST_SHOWN_NOTIFICATION_TYPE_KEY).apply();

        recordHistogram("Mobile.SystemNotification.BlockedAfterShown", lastType);
    }

    private static void recordHistogram(String name, @SystemNotificationType int type) {
        if (type == SystemNotificationType.UNKNOWN) return;

        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(name, type, SystemNotificationType.NUM_ENTRIES);
    }

    /**
     * Records the notification age, defined as the duration from the notification shown to the time
     * when an user interaction happens.
     * @param name The histogram name.
     * @param createTime The creation timestamp of the notification, generated by
     *                   {@link System#currentTimeMillis()}.
     */
    private static void recordNotificationAgeHistogram(String name, long createTime) {
        // If we didn't get shared preference data, do nothing.
        if (createTime == NotificationIntentInterceptor.INVALID_CREATE_TIME) return;

        int ageSample = (int) MathUtils.clamp(
                (System.currentTimeMillis() - createTime) / DateUtils.MINUTE_IN_MILLIS, 0,
                Integer.MAX_VALUE);
        new CachedMetrics
                .CustomCountHistogramSample(
                        name, 1, (int) (DateUtils.WEEK_IN_MILLIS / DateUtils.MINUTE_IN_MILLIS), 50)
                .record(ageSample);
    }
}
