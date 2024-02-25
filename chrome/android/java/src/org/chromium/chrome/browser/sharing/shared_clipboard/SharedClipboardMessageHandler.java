// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.shared_clipboard;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.sharing.SharingNotificationUtil;
import org.chromium.chrome.browser.sharing.SharingSendMessageResult;
import org.chromium.chrome.browser.sharing.SharingServiceProxy;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;

/** Handles Shared Clipboard messages and notifications for Android. */
public class SharedClipboardMessageHandler {
    private static final String EXTRA_DEVICE_GUID = "SharedClipboard.EXTRA_DEVICE_GUID";
    private static final String EXTRA_DEVICE_CLIENT_NAME =
            "SharedClipboard.EXTRA_DEVICE_CLIENT_NAME";

    /** Handles the tapping of an incoming notification when text is shared with current device. */
    public static final class TapReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            // TODO(mvanouwerkerk): handle this.
        }
    }

    /** Handles the tapping of an error notification which retries sharing text. */
    public static final class TryAgainReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            SharingNotificationUtil.dismissNotification(
                    NotificationConstants.GROUP_SHARED_CLIPBOARD,
                    NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING);
            String guid = IntentUtils.safeGetStringExtra(intent, EXTRA_DEVICE_GUID);
            String name = IntentUtils.safeGetStringExtra(intent, EXTRA_DEVICE_CLIENT_NAME);
            String text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);

            showSendingNotification(guid, name, text);
        }
    }

    /**
     * Shows the ongoing sending notification when SharingMessage is being sent for shared
     * clipboard. If successful, the notification is dismissed. Otherwise an error notification pops
     * up.
     * @param guid The guid of the receiver device.
     * @param name The name of the receiver device.
     * @param text The text shared from the sender device.
     */
    public static void showSendingNotification(String guid, String name, String text) {
        if (TextUtils.isEmpty(guid) || TextUtils.isEmpty(name) || TextUtils.isEmpty(text)) {
            return;
        }

        SharingNotificationUtil.showSendingNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING,
                name);

        // After this point the native browser process must be fully initialized in order to use
        // the profile, which is accessed by SharingServiceProxy. It might not yet be fully
        // initialized if a user clicked retry on the error notification after sending a shared
        // clipboard message failed, and Chrome was killed in the meantime.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        // TODO(crbug.com/1015411): Wait for device info in a more central place.
        SharingServiceProxy.getInstance()
                .addDeviceCandidatesInitializedObserver(
                        () -> onDeviceCandidatesInitialized(guid, text, name));
    }

    private static void onDeviceCandidatesInitialized(String guid, String text, String name) {
        SharingServiceProxy.getInstance()
                .sendSharedClipboardMessage(
                        guid,
                        text,
                        result -> onSharedClipboardMessageResult(guid, text, name, result));
    }

    private static void onSharedClipboardMessageResult(
            String guid, String text, String name, Integer result) {
        if (result == SharingSendMessageResult.SUCCESSFUL) {
            SharingNotificationUtil.dismissNotification(
                    NotificationConstants.GROUP_SHARED_CLIPBOARD,
                    NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING);
            return;
        }
        String contentTitle = getErrorNotificationTitle(result);
        String contentText = getErrorNotificationText(result, name);
        PendingIntentProvider tryAgainIntent = null;

        if (result == SharingSendMessageResult.ACK_TIMEOUT
                || result == SharingSendMessageResult.NETWORK_ERROR) {
            Context context = ContextUtils.getApplicationContext();
            tryAgainIntent =
                    PendingIntentProvider.getBroadcast(
                            context,
                            /* requestCode= */ 0,
                            new Intent(context, TryAgainReceiver.class)
                                    .putExtra(Intent.EXTRA_TEXT, text)
                                    .putExtra(EXTRA_DEVICE_GUID, guid)
                                    .putExtra(EXTRA_DEVICE_CLIENT_NAME, name),
                            PendingIntent.FLAG_UPDATE_CURRENT);
        }

        SharingNotificationUtil.showSendErrorNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_OUTGOING,
                contentTitle,
                contentText,
                tryAgainIntent);
    }

    /**
     * Return the title of error notification shown based on result of send message to other device.
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
                        R.string
                                .browser_sharing_shared_clipboard_error_dialog_title_payload_too_large);

            case SharingSendMessageResult.INTERNAL_ERROR:
            case SharingSendMessageResult.ENCRYPTION_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_internal_error, contentType);

            default:
                assert false;
                return resources.getString(
                        R.string.browser_sharing_error_dialog_title_internal_error, contentType);
        }
    }

    /**
     * Returns the text of the error notification shown based on the result of sending a message to
     * another device.
     *
     * @param result The result of sending a message to another device.
     * @param name The name of the receiver device.
     * @return the text for the error notification.
     */
    private static String getErrorNotificationText(
            @SharingSendMessageResult int result, String name) {
        Resources resources = ContextUtils.getApplicationContext().getResources();

        switch (result) {
            case SharingSendMessageResult.DEVICE_NOT_FOUND:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_device_not_found, name);

            case SharingSendMessageResult.NETWORK_ERROR:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_network_error);

            case SharingSendMessageResult.PAYLOAD_TOO_LARGE:
                return resources.getString(
                        R.string
                                .browser_sharing_shared_clipboard_error_dialog_text_payload_too_large);

            case SharingSendMessageResult.ACK_TIMEOUT:
                return resources.getString(
                        R.string.browser_sharing_error_dialog_text_device_ack_timeout, name);

            case SharingSendMessageResult.INTERNAL_ERROR:
            case SharingSendMessageResult.ENCRYPTION_ERROR:
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
     *
     * @param name The name of the sender device.
     */
    @CalledByNative
    private static void showNotification(String name) {
        Context context = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent =
                PendingIntentProvider.getBroadcast(
                        context,
                        /* requestCode= */ 0,
                        new Intent(context, TapReceiver.class),
                        PendingIntent.FLAG_UPDATE_CURRENT);
        Resources resources = context.getResources();
        String notificationTitle =
                TextUtils.isEmpty(name)
                        ? resources.getString(
                                R.string.shared_clipboard_notification_title_unknown_device)
                        : resources.getString(R.string.shared_clipboard_notification_title, name);
        SharingNotificationUtil.showNotification(
                NotificationUmaTracker.SystemNotificationType.SHARED_CLIPBOARD,
                NotificationConstants.GROUP_SHARED_CLIPBOARD,
                NotificationConstants.NOTIFICATION_ID_SHARED_CLIPBOARD_INCOMING,
                contentIntent,
                /* deleteIntent= */ null,
                /* confirmIntent= */ null,
                /* cancelIntent= */ null,
                notificationTitle,
                resources.getString(R.string.shared_clipboard_notification_text),
                R.drawable.ic_devices_16dp,
                R.drawable.shared_clipboard_40dp,
                R.color.default_icon_color_accent1_baseline,
                /* startsActivity= */ false);
    }
}
