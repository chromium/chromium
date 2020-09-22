// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.annotation.TargetApi;
import android.app.IntentService;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.PersistableBundle;
import android.os.StrictMode;
import android.os.SystemClock;
import android.util.Log;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * The Notification service receives intents fired as responses to user actions issued on Android
 * notifications displayed in the notification tray.
 */
public class NotificationService extends IntentService {
    private static final String TAG = NotificationService.class.getSimpleName();

    /**
     * The class which receives the intents from the Android framework. It initializes the
     * Notification service, and forward the intents there. Declared public as it needs to be
     * initialized by the Android framework.
     */
    public static class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG, "Received a notification intent in the NotificationService's receiver.");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                // Android encourages us not to start services directly on N+, so instead we
                // schedule a job to handle the notification intent. We use the Android JobScheduler
                // rather than GcmNetworkManager or FirebaseJobDispatcher since the JobScheduler
                // allows us to execute immediately by setting an override deadline of zero
                // milliseconds.
                JobScheduler scheduler =
                        (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
                PersistableBundle extras = NotificationJobService.getJobExtrasFromIntent(intent);
                putJobScheduledTimeInExtras(extras);
                JobInfo job =
                        new JobInfo
                                .Builder(TaskIds.NOTIFICATION_SERVICE_JOB_ID,
                                        new ComponentName(context, NotificationJobService.class))
                                .setExtras(extras)
                                .setOverrideDeadline(0)
                                .build();
                scheduler.schedule(job);
            } else {
                // TODO(peter): Do we need to acquire a wake lock here?

                intent.setClass(context, NotificationService.class);
                context.startService(intent);
            }
        }

        @TargetApi(Build.VERSION_CODES.N)
        private static void putJobScheduledTimeInExtras(PersistableBundle extras) {
            extras.putLong(NotificationConstants.EXTRA_JOB_SCHEDULED_TIME_MS,
                    SystemClock.elapsedRealtime());
        }
    }

    public NotificationService() {
        super(TAG);
    }

    /**
     * Called when a Notification has been interacted with by the user. If we can verify that
     * the Intent has a notification Id, start Chrome (if needed) on the UI thread.
     *
     * @param intent The intent containing the specific information.
     */
    @Override
    public void onHandleIntent(final Intent intent) {
        if (!intent.hasExtra(NotificationConstants.EXTRA_NOTIFICATION_ID)
                || !intent.hasExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN)) {
            return;
        }

        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> { dispatchIntentOnUIThread(intent); });
    }

    /**
     * Initializes Chrome and starts the browser process if it's not running as of yet, and
     * dispatch |intent| to the NotificationPlatformBridge once this is done.
     *
     * @param intent The intent containing the notification's information.
     */
    static void dispatchIntentOnUIThread(Intent intent) {
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        // Warm up the WebappRegistry, as we need to check if this notification should launch a
        // standalone web app. This no-ops if the registry is already initialized and warmed,
        // but triggers a strict mode violation otherwise (i.e. the browser isn't running).
        // Temporarily disable strict mode to work around the violation.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            WebappRegistry.getInstance();
            WebappRegistry.warmUpSharedPrefs();
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        // Now that the browser process is initialized, we pass forward the call to the
        // NotificationPlatformBridge which will take care of delivering the appropriate events.
        if (!NotificationPlatformBridge.dispatchNotificationEvent(intent)) {
            Log.w(TAG, "Unable to dispatch the notification event to Chrome.");
        }

        // TODO(peter): Verify that the lifetime of the NotificationService is sufficient
        // when a notification event could be dispatched successfully.
    }
}
