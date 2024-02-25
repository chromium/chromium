// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import org.chromium.base.FeatureList;
import org.chromium.base.Log;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;

import java.util.concurrent.TimeUnit;

/** Flag configuration for Price Tracking Notification experience. */
public class PriceTrackingNotificationConfig {
    private static final String TAG = "PriceTrackNotif";
    private static final String NOTIFICATION_TIMEOUT_PARAM = "notification_timeout_ms";
    private static final String NOTIFICATION_TIMESTAMPS_STORE_WINDOW_PARAM =
            "notification_timestamps_store_window_ms";
    private static final String CHROME_MANAGED_NOTIFICATION_MAX_NUMBER_PARAM =
            "chrome_managed_notification_max_number";
    private static final String USER_MANAGED_NOTIFICATION_MAX_NUMBER_PARAM =
            "user_managed_notification_max_number";

    // Gets the timeout of the price drop notification.
    public static int getNotificationTimeoutMs() {
        int defaultTimeout = (int) TimeUnit.HOURS.toMillis(3);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    NOTIFICATION_TIMEOUT_PARAM,
                    defaultTimeout);
        }
        return defaultTimeout;
    }

    // Gets the window within which we store the notification shown timestamps.
    public static int getNotificationTimestampsStoreWindowMs() {
        int defaultWindow = (int) TimeUnit.DAYS.toMillis(1);
        if (FeatureList.isInitialized()) {
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                    NOTIFICATION_TIMESTAMPS_STORE_WINDOW_PARAM,
                    defaultWindow);
        }
        return defaultWindow;
    }

    // Gets the max allowed number of notifications for specified type in given window.
    public static int getMaxAllowedNotificationNumber(@SystemNotificationType int type) {
        int defaultNumber = 4;
        if (FeatureList.isInitialized()) {
            final String param;
            if (type == SystemNotificationType.PRICE_DROP_ALERTS_CHROME_MANAGED) {
                param = CHROME_MANAGED_NOTIFICATION_MAX_NUMBER_PARAM;
            } else if (type == SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED) {
                param = USER_MANAGED_NOTIFICATION_MAX_NUMBER_PARAM;
            } else {
                Log.e(TAG, "Invalid notification type.");
                return defaultNumber;
            }
            return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.COMMERCE_PRICE_TRACKING, param, defaultNumber);
        }
        return defaultNumber;
    }
}
