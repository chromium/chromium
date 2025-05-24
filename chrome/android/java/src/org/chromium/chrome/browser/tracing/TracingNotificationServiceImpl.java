// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.IntentUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tracing.settings.TracingSettings;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Service that handles the actions on tracing notifications. */
public class TracingNotificationServiceImpl extends TracingNotificationService.Impl {
    private static final String ACTION_STOP_RECORDING =
            "org.chromium.chrome.browser.tracing.STOP_RECORDING";

    private static final String ACTION_DISCARD_TRACE =
            "org.chromium.chrome.browser.tracing.DISCARD_TRACE";

    /**
     * Get the intent to send to stop a trace recording.
     *
     * @param context the application's context.
     * @return the intent.
     */
    public static PendingIntent getStopRecordingIntent(Context context) {
        Intent intent = new Intent(context, TracingNotificationService.class);
        intent.setAction(ACTION_STOP_RECORDING);
        return PendingIntent.getService(
                context,
                0,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /**
     * Get the intent to discard a recorded trace.
     *
     * @param context the application's context.
     * @return the intent.
     */
    public static PendingIntent getDiscardTraceIntent(Context context) {
        Intent intent = new Intent(context, TracingNotificationService.class);
        intent.setAction(ACTION_DISCARD_TRACE);
        return PendingIntent.getService(
                context,
                0,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    /**
     * Get the intent to open the settings tab with a "share trace" button.
     *
     * @param context the application's context.
     * @return the intent.
     */
    public static PendingIntent getOpenSettingsIntent(Context context) {
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent intent = settingsNavigation.createSettingsIntent(context, TracingSettings.class);
        return PendingIntent.getActivity(
                context,
                0,
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT
                        | IntentUtils.getPendingIntentMutabilityFlag(false));
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        PostTask.runSynchronously(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Clear the notification but don't do anything else if the TracingController
                    // went away.
                    if (!TracingController.isInitialized()) {
                        TracingNotificationManager.dismissNotification();
                        return;
                    }

                    if (ACTION_STOP_RECORDING.equals(intent.getAction())) {
                        TracingController.getInstance().stopRecording();
                    } else if (ACTION_DISCARD_TRACE.equals(intent.getAction())) {
                        TracingController.getInstance().discardTrace();
                    }
                });
    }
}
