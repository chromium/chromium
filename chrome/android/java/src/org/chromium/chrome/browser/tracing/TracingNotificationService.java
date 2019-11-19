// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tracing;

import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Service that handles the actions on tracing notifications.
 */
public class TracingNotificationService extends IntentService {
    private static final String TAG = "tracing_notification";

    private static final String ACTION_STOP_RECORDING =
            "org.chromium.chrome.browser.tracing.STOP_RECORDING";

    private static final String ACTION_DISCARD_TRACE =
            "org.chromium.chrome.browser.tracing.DISCARD_TRACE";

    private static final String ACTION_SHARE_TRACE =
            "org.chromium.chrome.browser.tracing.SHARE_TRACE";

    /**
     * Get the intent to send to stop a trace recording.
     *
     * @param context the application's context.
     * @return the intent.
     */
    public static PendingIntent getStopRecordingIntent(Context context) {
        Intent intent = new Intent(context, TracingNotificationService.class);
        intent.setAction(ACTION_STOP_RECORDING);
        return PendingIntent.getService(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
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
        return PendingIntent.getService(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Get the intent to share a recorded trace.
     *
     * @param context the application's context.
     * @return the intent.
     */
    public static PendingIntent getShareTraceIntent(Context context) {
        Intent intent = new Intent(context, TracingNotificationService.class);
        intent.setAction(ACTION_SHARE_TRACE);
        return PendingIntent.getService(context, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    }

    /**
     * Construct the service. Called by Android.
     */
    public TracingNotificationService() {
        super(TAG);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            // Clear the notification but don't do anything else if the TracingController went away.
            if (!TracingController.isInitialized()) {
                TracingNotificationManager.dismissNotification();
                return;
            }

            if (ACTION_STOP_RECORDING.equals(intent.getAction())) {
                TracingController.getInstance().stopRecording();
            } else if (ACTION_SHARE_TRACE.equals(intent.getAction())) {
                TracingController.getInstance().shareTrace();
            } else if (ACTION_DISCARD_TRACE.equals(intent.getAction())) {
                TracingController.getInstance().discardTrace();
            }
        });
    }
}
