// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.sms_fetcher;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/**
 * Handles Sms Fetcher messages and notifications for Android.
 */
public class SmsFetcherMessageHandler {
    private static final String NOTIFICATION_ACTION_TAP = "sms_fetcher_notification.tap";
    private static final String NOTIFICATION_ACTION_DISMISS = "sms_fetcher_notification.dismiss";
    private static final String TAG = "SmsMessageHandler";
    private static final boolean DEBUG = false;
    private static long sSmsFetcherMessageHandlerAndroid;
    private static String sOrigin;

    /**
     * Handles the interaction of an incoming notification when an expected SMS arrives.
     */
    public static final class NotificationReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            boolean nativeIsDestroyed = sSmsFetcherMessageHandlerAndroid == 0;
            RecordHistogram.recordBooleanHistogram(
                    "Sharing.SmsFetcherTapWithChromeDestroyed", nativeIsDestroyed);
            // This could happen if the user manually swipes away Chrome from the task switcher or
            // the OS decides to destroy Chrome due to lack of memory etc. In these cases we just
            // close the notification.
            if (nativeIsDestroyed) return;
            switch (action) {
                case NOTIFICATION_ACTION_TAP:
                    if (DEBUG) Log.d(TAG, "Notification tapped");
                    SmsFetcherMessageHandlerJni.get().onConfirm(
                            sSmsFetcherMessageHandlerAndroid, sOrigin);
                    break;
                case NOTIFICATION_ACTION_DISMISS:
                    if (DEBUG) Log.d(TAG, "Notification dismissed");
                    SmsFetcherMessageHandlerJni.get().onDismiss(
                            sSmsFetcherMessageHandlerAndroid, sOrigin);
                    break;
            }
        }
    }

    /**
     * Ask the user to tap the notification to verify the sms verification code.
     *
     * @param oneTimeCode The one time code from SMS
     * @param origin The origin from the SMS
     * @param smsFetcherMessageHandlerAndroid The native handler
     */
    @CalledByNative
    private static void showNotification(
            String oneTimeCode, String origin, long smsFetcherMessageHandlerAndroid) {
        sOrigin = origin;
        sSmsFetcherMessageHandlerAndroid = smsFetcherMessageHandlerAndroid;
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, NotificationReceiver.class).setAction(NOTIFICATION_ACTION_TAP),
                PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntentProvider deleteIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, NotificationReceiver.class)
                        .setAction(NOTIFICATION_ACTION_DISMISS),
                PendingIntent.FLAG_UPDATE_CURRENT);
        Resources resources = context.getResources();
        String notificationText =
                resources.getString(R.string.sms_fetcher_notification_text, oneTimeCode, origin);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.SMS_FETCHER,
                NotificationConstants.GROUP_SMS_FETCHER,
                NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING, contentIntent,
                deleteIntent, resources.getString(R.string.sms_fetcher_notification_title),
                notificationText, R.drawable.ic_devices_48dp, R.drawable.infobar_chrome,
                R.color.infobar_icon_drawable_color, /*startsActivity=*/false);
    }

    @CalledByNative
    private static void dismissNotification() {
        SharingNotificationUtil.dismissNotification(NotificationConstants.GROUP_SMS_FETCHER,
                NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING);
    }

    @CalledByNative
    private static void reset() {
        sSmsFetcherMessageHandlerAndroid = 0;
        sOrigin = "";
    }

    @NativeMethods
    interface Natives {
        void onConfirm(long nativeSmsFetchRequestHandler, String origin);
        void onDismiss(long nativeSmsFetchRequestHandler, String origin);
    }
}
