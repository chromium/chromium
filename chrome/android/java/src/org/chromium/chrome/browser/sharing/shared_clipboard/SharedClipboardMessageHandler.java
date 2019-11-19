// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.shared_clipboard;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.chrome.browser.sharing.SharingSendMessageResult;
import org.chromium.chrome.browser.sharing.SharingServiceProxy;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Handles Shared Clipboard messages and notifications for Android.
 */
public class SharedClipboardMessageHandler {
    private static final String EXTRA_DEVICE_GUID = "SharedClipboard.EXTRA_DEVICE_GUID";
    private static final String EXTRA_DEVICE_CLIENT_NAME =
            "SharedClipboard.EXTRA_DEVICE_CLIENT_NAME";

    /**
     * Handles the tapping of an incoming notification when text is shared with current device.
     */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // TODO(mvanouwerkerk): handle this.
        }
    }

    /**
     * Handles the tapping of an error notification which retries sharing text.
     */
    public static final class TryAgainReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            SharingNotificationUtil.dismissNotification(
                    NotificationConstants.GROUP_SHARED_CLIPBOARD,
                    NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING);
            String text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);
            String deviceName = IntentUtils.safeGetStringExtra(intent, EXTRA_DEVICE_CLIENT_NAME);
            String deviceGuid = IntentUtils.safeGetStringExtra(intent, EXTRA_DEVICE_GUID);

            showSendingNotification(deviceGuid, deviceName, text);
        }
    }

    /**
     * Shows the ongoing sending notification when SharingMessage is being sent for shared
     * clipboard. If successful, the notification is dismissed. Otherwise an error notification pops
     * up.
     * @param deviceGuid The guid of the device we are sharing with.
     * @param deviceName The name of the device we are sharing with.
     * @param text The text shared from current device.
     */
    public static void showSendingNotification(String deviceGuid, String deviceName, String text) {
        if (TextUtils.isEmpty(deviceGuid) || TextUtils.isEmpty(deviceName)
                || TextUtils.isEmpty(text)) {
            return;
        }

        SharingNotificationUtil.showSendingNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING, deviceName);

        SharingServiceProxy.getInstance().sendSharedClipboardMessage(deviceGuid, text, result -> {
            if (result == SharingSendMessageResult.SUCCESSFUL) {
                SharingNotificationUtil.dismissNotification(
                        NotificationConstants.GROUP_SHARED_CLIPBOARD,
                        NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING);
            } else {
                String contentTitle = getErrorNotificationTitle(result);
                String contentText = getErrorNotificationText(result, deviceName);
                PendingIntentProvider tryAgainIntent = null;

                if (result == SharingSendMessageResult.ACK_TIMEOUT
                        || result == SharingSendMessageResult.NETWORK_ERROR) {
                    Context context = ContextUtils.getApplicationContext();
                    tryAgainIntent = PendingIntentProvider.getBroadcast(context, /*requestCode=*/0,
                            new Intent(context, TryAgainReceiver.class)
                                    .putExtra(Intent.EXTRA_TEXT, text)
                                    .putExtra(EXTRA_DEVICE_GUID, deviceGuid)
                                    .putExtra(EXTRA_DEVICE_CLIENT_NAME, deviceName),
                            PendingIntent.FLAG_UPDATE_CURRENT);
                }

                SharingNotificationUtil.showSendErrorNotification(
                        NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                        NotificationConstants.GROUP_SHARED_CLIPBOARD,
                        NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING,
                        contentTitle, contentText, tryAgainIntent);
            }
        });
    }

    /**
     * Return the title of error notification shown based on result of send message to other device.
     * TODO(himanshujaju) - All text except PAYLOAD_TOO_LARGE are common across features. Extract
     * them out when next feature is added.
     *
     * @param result The result of sending message to other device.
     * @return the title for error notification.
     */
    private static String getErrorNotificationTitle(@SharingSendMessageResult int result) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        String contentType = resources.getString(R.string.browser_sharing_content_type_text);

        switch (result) {
            case SharingSendMessageResult.DEVICE_NOT_FOUND:
            case SharingSendMessageResult.NETWORK_ERROR:
            case SharingSendMessageResult.ACK_TIMEOUT:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_generic_error, contentType);

            case SharingSendMessageResult.PAYLOAD_TOO_LARGE:
                return resources.getString(
                        R.string.browser_sharing_shared_clipboard_error_dialog_title_payload_too_large);

            case SharingSendMessageResult.INTERNAL_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_internal_error, contentType);

            default:
                assert false;
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_internal_error, contentType);
        }
    }

    /**
     * Return the text of error notification shown based on result of send message to other device.
     * TODO(himanshujaju) - All text except PAYLOAD_TOO_LARGE are common across features. Extract
     * them out when next feature is added.
     *
     * @param result The result of sending message to other device.
     * @return the text for error notification.
     */
    private static String getErrorNotificationText(
            @SharingSendMessageResult int result, String targetDeviceName) {
        Resources resources = ContextUtils.getApplicationContext().getResources();

        switch (result) {
            case SharingSendMessageResult.DEVICE_NOT_FOUND:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_device_not_found,
                        targetDeviceName);

            case SharingSendMessageResult.NETWORK_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_network_error);

            case SharingSendMessageResult.PAYLOAD_TOO_LARGE:
                return resources.getString(
                        R.string.browser_sharing_shared_clipboard_error_dialog_text_payload_too_large);

            case SharingSendMessageResult.ACK_TIMEOUT:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_device_ack_timeout,
                        targetDeviceName);

            case SharingSendMessageResult.INTERNAL_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_internal_error);

            default:
                assert false;
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_internal_error);
        }
    }

    /**
     * Displays a notification to tell the user that new clipboard contents have been received and
     * written to the clipboard.
     */
    @CalledByNative
    private static void showNotification(String deviceName) {
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent = PendingIntentProvider.getBroadcast(context,
                /*requestCode=*/0, new Intent(context, TapReceiver.class),
                PendingIntent.FLAG_UPDATE_CURRENT);
        Resources resources = context.getResources();
        String notificationTitle = TextUtils.isEmpty(deviceName)
                ? resources.getString(R.string.shared_clipboard_notification_title_unknown_device)
                : resources.getString(R.string.shared_clipboard_notification_title, deviceName);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_INCOMING, contentIntent,
                notificationTitle, resources.getString(R.string.shared_clipboard_notification_text),
                R.drawable.ic_devices_16dp, R.drawable.shared_clipboard_40dp,
                R.color.default_icon_color_blue);
    }
}
