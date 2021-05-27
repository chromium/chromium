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
    private static final String NOTIFICATION_ACTION_CONFIRM = "sms_fetcher_notification.confirm";
    private static final String NOTIFICATION_ACTION_CANCEL = "sms_fetcher_notification.cancel";
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
            SharingNotificationUtil.dismissNotification(NotificationConstants.GROUP_SMS_FETCHER,
                    NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING);
            // This could happen if the user manually swipes away Chrome from the task switcher or
            // the OS decides to destroy Chrome due to lack of memory etc. In these cases we just
            // close the notification.
            if (nativeIsDestroyed) return;
            switch (action) {
                case NOTIFICATION_ACTION_CONFIRM:
                    if (DEBUG) Log.d(TAG, "Notification confirmed");
                    SmsFetcherMessageHandlerJni.get().onConfirm(
                            sSmsFetcherMessageHandlerAndroid, sOrigin);
                    break;
                case NOTIFICATION_ACTION_CANCEL:
                    if (DEBUG) Log.d(TAG, "Notification canceled");
                    SmsFetcherMessageHandlerJni.get().onDismiss(
                            sSmsFetcherMessageHandlerAndroid, sOrigin);
                    break;
            }
        }
    }

    /**
     * Ask users to interact with the notification to allow Chrome to submit the code to the remote
     * device.
     *
     * @param oneTimeCode The one time code from SMS
     * @param origin The origin from the SMS
     * @param remoteOs The OS name where the remote request comes from
     * @param smsFetcherMessageHandlerAndroid The native handler
     */
    @CalledByNative
    private static void showNotification(String oneTimeCode, String origin, String remoteOs,
            long smsFetcherMessageHandlerAndroid) {
        sOrigin = origin;
        sSmsFetcherMessageHandlerAndroid = smsFetcherMessageHandlerAndroid;
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider confirmIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, NotificationReceiver.class)
                        .setAction(NOTIFICATION_ACTION_CONFIRM),
                PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntentProvider cancelIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0,
                new Intent(context, NotificationReceiver.class)
                        .setAction(NOTIFICATION_ACTION_CANCEL),
                PendingIntent.FLAG_UPDATE_CURRENT);
        Resources resources = context.getResources();
        String notificationTitle = remoteOs.equals("")
                ? resources.getString(R.string.sms_fetcher_notification_title_unknown_device)
                : resources.getString(R.string.sms_fetcher_notification_title, remoteOs);
        String notificationText =
                resources.getString(R.string.sms_fetcher_notification_text, oneTimeCode, origin);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.SMS_FETCHER,
                NotificationConstants.GROUP_SMS_FETCHER,
                NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING, /*contentIntent=*/null,
                /*deleteIntent=*/cancelIntent, confirmIntent, cancelIntent, notificationTitle,
                notificationText, R.drawable.ic_devices_48dp, R.drawable.infobar_chrome,
                R.color.infobar_icon_drawable_color,
                /*startsActivity=*/false);
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
