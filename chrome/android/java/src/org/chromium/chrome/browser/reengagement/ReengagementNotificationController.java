// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reengagement;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DefaultBrowserInfo2;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Controller to manage when and how we show re-engagement notifications to users.
 * TODO(crbug.com/40140907): Modularize this file.
 */
public class ReengagementNotificationController {
    /** An {@link Intent} action to open Chrome to the NTP. */
    public static final String LAUNCH_NTP_ACTION = "launch_ntp";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected static final String NOTIFICATION_TAG = "reengagement_notification";

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected static final int NOTIFICATION_ID = 200;

    private final Context mContext;
    private final Tracker mTracker;
    private final Class<? extends Activity> mActivityClazz;

    /** @return Whether or not the re-engagement notification is enabled. */
    public static boolean isEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.REENGAGEMENT_NOTIFICATION);
    }

    /** Creates an instance of ReengagementNotificationController. */
    public ReengagementNotificationController(
            Context context, Tracker tracker, Class<? extends Activity> activityClazz) {
        mContext = context;
        mTracker = tracker;
        mActivityClazz = activityClazz;
    }

    /** Attempt to re-engage the user by showing a notification (if criteria are met). */
    public void tryToReengageTheUser() {
        if (!isEnabled()) return;
        getDefaultBrowserInfo(
                info -> {
                    if (info == null || info.browserCount <= 1) return;

                    if (showNotification(
                            FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE)) {
                        return;
                    }
                    if (showNotification(
                            FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE)) {
                        return;
                    }
                    if (showNotification(
                            FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE)) {
                        return;
                    }
                });
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void getDefaultBrowserInfo(Callback<DefaultBrowserInfo2.DefaultInfo> callback) {
        DefaultBrowserInfo2.getDefaultBrowserInfo(callback);
    }

    private boolean showNotification(String feature) {
        @StringRes int titleId = 0;
        @StringRes int descriptionId = 0;
        @SystemNotificationType int notificationUmaType = SystemNotificationType.UNKNOWN;

        if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE)) {
            titleId = R.string.chrome_reengagement_notification_1_title;
            descriptionId = R.string.chrome_reengagement_notification_1_description;
            notificationUmaType = SystemNotificationType.CHROME_REENGAGEMENT_1;
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE)) {
            titleId = R.string.chrome_reengagement_notification_2_title;
            descriptionId = R.string.chrome_reengagement_notification_2_description;
            notificationUmaType = SystemNotificationType.CHROME_REENGAGEMENT_2;
        } else if (TextUtils.equals(
                feature, FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE)) {
            titleId = R.string.chrome_reengagement_notification_3_title;
            descriptionId = R.string.chrome_reengagement_notification_3_description;
            notificationUmaType = SystemNotificationType.CHROME_REENGAGEMENT_3;
        } else {
            return false;
        }

        if (!mTracker.shouldTriggerHelpUI(feature)) return false;
        mTracker.dismissed(feature);

        NotificationMetadata metadata =
                new NotificationMetadata(notificationUmaType, NOTIFICATION_TAG, NOTIFICATION_ID);
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChannelId.UPDATES, metadata);

        Intent intent = new Intent(mContext, mActivityClazz);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setAction(LAUNCH_NTP_ACTION);

        PendingIntentProvider intentProvider =
                PendingIntentProvider.getActivity(
                        mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

        builder.setContentTitle(mContext.getString(titleId))
                .setContentText(mContext.getString(descriptionId))
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentIntent(intentProvider)
                .setAutoCancel(true);

        BaseNotificationManagerProxy notificationManager =
                BaseNotificationManagerProxyFactory.create(mContext);
        NotificationWrapper notification = builder.buildNotificationWrapper();
        notificationManager.notify(notification);

        NotificationUmaTracker.getInstance()
                .onNotificationShown(notificationUmaType, notification.getNotification());
        return true;
    }
}
