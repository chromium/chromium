// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.click_to_call;

import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/** Manages ClickToCall related notifications for Android. */
@NullMarked
public class ClickToCallMessageHandler {
    private static final String EXTRA_PHONE_NUMBER = "ClickToCallMessageHandler.EXTRA_PHONE_NUMBER";

    /**
     * Opens the dialer with the |phoneNumber| already prefilled.
     *
     * @param phoneNumber The phone number to show in the dialer.
     */
    private static void openDialer(@Nullable String phoneNumber) {
        try {
            ContextUtils.getApplicationContext().startActivity(getDialIntent(phoneNumber));
            ClickToCallUma.recordDialerPresent(true);
        } catch (ActivityNotFoundException activityNotFound) {
            // Notify the user that no dialer app was available.
            ClickToCallUma.recordDialerPresent(false);
            displayDialerNotFoundNotification();
        }
    }

    /**
     * Shows an error notification suggesting the user to enable a dialer app to
     * use click to call.
     */
    public static void displayDialerNotFoundNotification() {
        Context context = ContextUtils.getApplicationContext();

        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.CLICK_TO_CALL,
                NotificationConstants.GROUP_CLICK_TO_CALL,
                NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL_ERROR,
                /* contentIntent= */ null,
                /* deleteIntent= */ null,
                /* confirmIntent= */ null,
                /* cancelIntent= */ null,
                context.getString(R.string.click_to_call_dialer_absent_notification_title),
                context.getString(R.string.click_to_call_dialer_absent_notification_text),
                R.drawable.ic_error_outline_red_24dp,
                R.drawable.ic_dialer_not_found_red_40dp,
                R.color.google_red_600,
                /* startsActivity= */ false);
    }

    /**
     * Handles the tapping of a notification by opening the dialer with the
     * phone number specified in the notification.
     */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            openDialer(IntentUtils.safeGetStringExtra(intent, EXTRA_PHONE_NUMBER));
        }
    }

    /**
     * Displays a notification that opens the dialer when clicked.
     *
     * @param phoneNumber The phone number to show in the dialer when the user taps the
     *     notification.
     */
    private static void displayNotification(String phoneNumber) {
        Context context = ContextUtils.getApplicationContext();
        String contentTitle = Uri.decode(phoneNumber);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.CLICK_TO_CALL,
                NotificationConstants.GROUP_CLICK_TO_CALL,
                NotificationConstants.NOTIFICATION_ID_CLICK_TO_CALL,
                getContentIntentProvider(phoneNumber),
                /* deleteIntent= */ null,
                /* confirmIntent= */ null,
                /* cancelIntent= */ null,
                contentTitle,
                context.getString(R.string.click_to_call_notification_text),
                R.drawable.ic_devices_16dp,
                R.drawable.ic_dialer_icon_blue_40dp,
                R.color.default_icon_color_accent1_baseline,
                /* startsActivity= */ true);
    }

    private static Intent getDialIntent(@Nullable String phoneNumber) {
        Intent dialIntent =
                TextUtils.isEmpty(phoneNumber)
                        ? new Intent(Intent.ACTION_DIAL)
                        : new Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + phoneNumber));
        dialIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return dialIntent;
    }

    private static PendingIntentProvider getContentIntentProvider(String phoneNumber) {
        Context context = ContextUtils.getApplicationContext();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // We can't use the TapReceiver broadcast to start the dialer Activity starting in
            // Android S. Use the dial intent directly instead.
            return PendingIntentProvider.getActivity(
                    context,
                    /* requestCode= */ 0,
                    getDialIntent(phoneNumber),
                    PendingIntent.FLAG_UPDATE_CURRENT);
        }

        return PendingIntentProvider.getBroadcast(
                context,
                /* requestCode= */ 0,
                new Intent(context, TapReceiver.class).putExtra(EXTRA_PHONE_NUMBER, phoneNumber),
                PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Handles a phone number sent from another device.
     *
     * @param phoneNumber The phone number to call.
     */
    @CalledByNative
    @VisibleForTesting
    static void handleMessage(@JniType("std::string") String phoneNumber) {
        displayNotification(phoneNumber);
    }
}
