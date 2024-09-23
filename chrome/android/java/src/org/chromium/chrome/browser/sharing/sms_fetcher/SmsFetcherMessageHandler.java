// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.sms_fetcher;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/** Handles Sms Fetcher messages and notifications for Android. */
public class SmsFetcherMessageHandler {
    private static final String NOTIFICATION_ACTION_CONFIRM = "sms_fetcher_notification.confirm";
    private static final String NOTIFICATION_ACTION_CANCEL = "sms_fetcher_notification.cancel";
    private static final String TAG = "SmsMessageHandler";
    private static final boolean DEBUG = false;
    private static long sSmsFetcherMessageHandlerAndroid;
    private static String sTopOrigin;
    private static String sEmbeddedOrigin;

    /** Handles the interaction of an incoming notification when an expected SMS arrives. */
    public static final class NotificationReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            boolean nativeIsDestroyed = sSmsFetcherMessageHandlerAndroid == 0;
            RecordHistogram.recordBooleanHistogram(
                    "Sharing.SmsFetcherTapWithChromeDestroyed", nativeIsDestroyed);
            SharingNotificationUtil.dismissNotification(
                    NotificationConstants.GROUP_SMS_FETCHER,
                    NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING);
            // This could happen if the user manually swipes away Chrome from the task switcher or
            // the OS decides to destroy Chrome due to lack of memory etc. In these cases we just
            // close the notification.
            if (nativeIsDestroyed) return;
            switch (action) {
                case NOTIFICATION_ACTION_CONFIRM:
                    if (DEBUG) Log.d(TAG, "Notification confirmed");
                    SmsFetcherMessageHandlerJni.get()
                            .onConfirm(
                                    sSmsFetcherMessageHandlerAndroid, sTopOrigin, sEmbeddedOrigin);
                    break;
                case NOTIFICATION_ACTION_CANCEL:
                    if (DEBUG) Log.d(TAG, "Notification canceled");
                    SmsFetcherMessageHandlerJni.get()
                            .onDismiss(
                                    sSmsFetcherMessageHandlerAndroid, sTopOrigin, sEmbeddedOrigin);
                    break;
            }
        }
    }

    /**
     * Returns the notification title string.
     *
     * @param oneTimeCode The one time code from SMS
     * @param topOrigin The top frame origin from the SMS
     * @param embeddedOrigin The embedded frame origin from the SMS. Null if the SMS does not
     *         contain an iframe origin.
     * @param clientName The client name where the remote request comes from
     */
    private static String getNotificationTitle(
            String oneTimeCode, String topOrigin, String embeddedOrigin, String clientName) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_OTP_CROSS_DEVICE_SIMPLE_STRING)) {
            if (embeddedOrigin == null) {
                return resources.getString(
                        R.string.sms_fetcher_notification_title_simple_string,
                        oneTimeCode,
                        topOrigin);
            }
            return resources.getString(
                    R.string.sms_fetcher_notification_title_simple_string,
                    oneTimeCode,
                    embeddedOrigin);
        }
        return resources.getString(
                R.string.sms_fetcher_notification_title, oneTimeCode, clientName);
    }

    /**
     * Returns the notification text string.
     *
     * @param oneTimeCode The one time code from SMS
     * @param topOrigin The top frame origin from the SMS
     * @param embeddedOrigin The embedded frame origin from the SMS. Null if the SMS does not
     *         contain an iframe origin.
     * @param clientName The client name where the remote request comes from
     */
    private static String getNotificationText(
            String oneTimeCode, String topOrigin, String embeddedOrigin, String clientName) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_OTP_CROSS_DEVICE_SIMPLE_STRING)) {
            if (embeddedOrigin == null) return clientName;
            return topOrigin + " · " + clientName;
        }
        return embeddedOrigin == null
                ? resources.getString(R.string.sms_fetcher_notification_text, topOrigin)
                : resources.getString(
                        R.string.sms_fetcher_notification_text_for_embedded_frame,
                        topOrigin,
                        embeddedOrigin);
    }

    /**
     * Ask users to interact with the notification to allow Chrome to submit the code to the remote
     * device.
     *
     * @param oneTimeCode The one time code from SMS
     * @param topOrigin The top frame origin from the SMS
     * @param embeddedOrigin The embedded frame origin from the SMS. Null if the SMS does not
     *     contain an iframe origin.
     * @param clientName The client name where the remote request comes from
     * @param smsFetcherMessageHandlerAndroid The native handler
     */
    @CalledByNative
    private static void showNotification(
            @JniType("std::string") String oneTimeCode,
            String topOrigin,
            @Nullable String embeddedOrigin,
            @JniType("std::string") String clientName,
            long smsFetcherMessageHandlerAndroid) {
        sTopOrigin = topOrigin;
        sEmbeddedOrigin = embeddedOrigin;
        sSmsFetcherMessageHandlerAndroid = smsFetcherMessageHandlerAndroid;
        Context context = ContextUtils.getApplicationContext();
        RecordHistogram.recordBooleanHistogram(
                "Sharing.SmsFetcherScreenOnAndUnlocked",
                DeviceConditions.isCurrentlyScreenOnAndUnlocked(context));
        PendingIntentProvider confirmIntent =
                PendingIntentProvider.getBroadcast(
                        context,
                        /* requestCode= */ 0,
                        new Intent(context, NotificationReceiver.class)
                                .setAction(NOTIFICATION_ACTION_CONFIRM),
                        PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntentProvider cancelIntent =
                PendingIntentProvider.getBroadcast(
                        context,
                        /* requestCode= */ 0,
                        new Intent(context, NotificationReceiver.class)
                                .setAction(NOTIFICATION_ACTION_CANCEL),
                        PendingIntent.FLAG_UPDATE_CURRENT);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.SMS_FETCHER,
                NotificationConstants.GROUP_SMS_FETCHER,
                NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING,
                /* contentIntent= */ null,
                /* deleteIntent= */ cancelIntent,
                confirmIntent,
                cancelIntent,
                getNotificationTitle(oneTimeCode, topOrigin, embeddedOrigin, clientName),
                getNotificationText(oneTimeCode, topOrigin, embeddedOrigin, clientName),
                R.drawable.ic_chrome,
                /* largeIconId= */ 0,
                R.color.default_icon_color_accent1_baseline,
                /* startsActivity= */ false);
    }

    @CalledByNative
    private static void dismissNotification() {
        SharingNotificationUtil.dismissNotification(
                NotificationConstants.GROUP_SMS_FETCHER,
                NotificationConstants.NOTIFICATION_ID_SMS_FETCHER_INCOMING);
    }

    @CalledByNative
    private static void reset() {
        sSmsFetcherMessageHandlerAndroid = 0;
        sTopOrigin = null;
        sEmbeddedOrigin = null;
    }

    @NativeMethods
    interface Natives {
        void onConfirm(long nativeSmsFetchRequestHandler, String topOrigin, String embeddedOrigin);

        void onDismiss(long nativeSmsFetchRequestHandler, String topOrigin, String embeddedOrigin);
    }
}
