// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.ui.accessibility.AccessibilityState;

/** Manages notifications displayed while tracing and once tracing is complete. */
public class TracingNotificationManager {
    private static final String TRACING_NOTIFICATION_TAG = "tracing_status";
    private static final int TRACING_NOTIFICATION_ID = 100;

    private static NotificationManagerProxy sNotificationManagerOverride;
    private static NotificationWrapperBuilder sTracingActiveNotificationBuilder;
    private static int sTracingActiveNotificationBufferPercentage;

    // Non-translated strings:
    private static final String MSG_ACTIVE_NOTIFICATION_TITLE = "Chrome trace is being recorded";
    private static final String MSG_ACTIVE_NOTIFICATION_ACCESSIBILITY_MESSAGE =
            "Tracing is active.";
    private static final String MSG_ACTIVE_NOTIFICATION_MESSAGE = "Trace buffer usage: %s%%";
    private static final String MSG_STOPPING_NOTIFICATION_TITLE = "Chrome trace is stopping";
    private static final String MSG_STOPPING_NOTIFICATION_MESSAGE =
            "Trace data is being collected and compressed.";
    private static final String MSG_COMPLETE_NOTIFICATION_TITLE = "Chrome trace is complete";
    private static final String MSG_COMPLETE_NOTIFICATION_MESSAGE =
            "The trace is ready. Open tracing settings to share.";
    private static final String MSG_STOP = "Stop recording";
    private static final String MSG_OPEN_SETTINGS = "Open tracing settings";

    // TODO(eseckler): Consider recording UMAs, see e.g. IncognitoNotificationManager.

    private static NotificationManagerProxy getNotificationManager(Context context) {
        return sNotificationManagerOverride != null
                ? sNotificationManagerOverride
                : new NotificationManagerProxyImpl(context);
    }

    /**
     * Instruct the TracingNotificationManager to use a different NotificationManager during a test.
     *
     * @param notificationManager the manager to use instead.
     */
    public static void overrideNotificationManagerForTesting(
            NotificationManagerProxy notificationManager) {
        sNotificationManagerOverride = notificationManager;
    }

    /**
     * @return whether notifications posted to the BROWSER notification channel are enabled by
     * the user. True if the state can't be determined.
     */
    public static boolean browserNotificationsEnabled() {
        if (!getNotificationManager(ContextUtils.getApplicationContext())
                .areNotificationsEnabled()) {
            return false;
        }

        // On Android O and above, the BROWSER channel may have independently been disabled, too.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return notificationChannelEnabled(ChromeChannelDefinitions.ChannelId.BROWSER);
        }

        return true;
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private static boolean notificationChannelEnabled(String channelId) {
        NotificationChannel channel =
                getNotificationManager(ContextUtils.getApplicationContext())
                        .getNotificationChannel(channelId);
        // Can't determine the state if the channel doesn't exist, assume notifications are enabled.
        if (channel == null) return true;
        return channel.getImportance() != NotificationManager.IMPORTANCE_NONE;
    }

    /** Replace the tracing notification with one indicating that a trace is being recorded. */
    public static void showTracingActiveNotification() {
        Context context = ContextUtils.getApplicationContext();
        String title = MSG_ACTIVE_NOTIFICATION_TITLE;
        sTracingActiveNotificationBufferPercentage = 0;
        String message =
                String.format(
                        MSG_ACTIVE_NOTIFICATION_MESSAGE,
                        sTracingActiveNotificationBufferPercentage);

        // We can't update the notification if touch exploration is enabled as this may interfere
        // with selecting the stop button, so choose a different message.
        if (AccessibilityState.isTouchExplorationEnabled()) {
            message = MSG_ACTIVE_NOTIFICATION_ACCESSIBILITY_MESSAGE;
        }

        sTracingActiveNotificationBuilder =
                createNotificationWrapperBuilder()
                        .setContentTitle(title)
                        .setContentText(message)
                        .setOngoing(true)
                        .addAction(
                                R.drawable.ic_stop_white_24dp,
                                MSG_STOP,
                                TracingNotificationServiceImpl.getStopRecordingIntent(context));
        showNotification(sTracingActiveNotificationBuilder.build());
    }

    /**
     * Update the tracing notification that is shown while a trace is being recorded with the
     * current buffer utilization. Should only be called while the "tracing active" notification is
     * shown.
     *
     * @param bufferUsagePercentage buffer utilization as float between 0 and 1.
     */
    public static void updateTracingActiveNotification(float bufferUsagePercentage) {
        assert (sTracingActiveNotificationBuilder != null);
        Context context = ContextUtils.getApplicationContext();

        // Don't update the notification if touch exploration is enabled as this may interfere with
        // selecting the stop button.
        if (AccessibilityState.isTouchExplorationEnabled()) return;

        int newPercentage = Math.round(bufferUsagePercentage * 100);
        if (sTracingActiveNotificationBufferPercentage == newPercentage) return;
        sTracingActiveNotificationBufferPercentage = newPercentage;

        String message =
                String.format(
                        MSG_ACTIVE_NOTIFICATION_MESSAGE,
                        sTracingActiveNotificationBufferPercentage);

        sTracingActiveNotificationBuilder.setContentText(message);
        showNotification(sTracingActiveNotificationBuilder.build());
    }

    /** Replace the tracing notification with one indicating that a trace is being finalized. */
    public static void showTracingStoppingNotification() {
        String title = MSG_STOPPING_NOTIFICATION_TITLE;
        String message = MSG_STOPPING_NOTIFICATION_MESSAGE;

        NotificationWrapperBuilder builder =
                createNotificationWrapperBuilder()
                        .setContentTitle(title)
                        .setContentText(message)
                        .setOngoing(true);
        showNotification(builder.build());
    }

    /**
     * Replace the tracing notification with one indicating that a trace was recorded successfully.
     */
    public static void showTracingCompleteNotification() {
        Context context = ContextUtils.getApplicationContext();
        String title = MSG_COMPLETE_NOTIFICATION_TITLE;
        String message = MSG_COMPLETE_NOTIFICATION_MESSAGE;
        int noIcon = 0;

        NotificationWrapperBuilder builder =
                createNotificationWrapperBuilder()
                        .setContentTitle(title)
                        .setContentText(message)
                        .setOngoing(false)
                        .addAction(
                                noIcon,
                                MSG_OPEN_SETTINGS,
                                TracingNotificationServiceImpl.getOpenSettingsIntent(context))
                        .setDeleteIntent(
                                TracingNotificationServiceImpl.getDiscardTraceIntent(context));
        showNotification(builder.build());
    }

    /** Dismiss any active tracing notification if there is one. */
    public static void dismissNotification() {
        NotificationManagerProxy manager =
                getNotificationManager(ContextUtils.getApplicationContext());
        manager.cancel(TRACING_NOTIFICATION_TAG, TRACING_NOTIFICATION_ID);
        sTracingActiveNotificationBuilder = null;
    }

    private static NotificationWrapperBuilder createNotificationWrapperBuilder() {
        return NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                        ChromeChannelDefinitions.ChannelId.BROWSER)
                .setVisibility(Notification.VISIBILITY_PUBLIC)
                .setSmallIcon(R.drawable.ic_chrome)
                .setShowWhen(false)
                .setLocalOnly(true);
    }

    private static void showNotification(Notification notification) {
        NotificationManagerProxy manager =
                getNotificationManager(ContextUtils.getApplicationContext());
        manager.notify(TRACING_NOTIFICATION_TAG, TRACING_NOTIFICATION_ID, notification);
    }
}
