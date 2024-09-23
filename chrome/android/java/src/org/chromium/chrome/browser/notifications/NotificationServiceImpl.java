// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.PersistableBundle;
import android.os.SystemClock;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.webapps.WebappsUtils;

/**
 * The Notification service receives intents fired as responses to user actions issued on Android
 * notifications displayed in the notification tray.
 */
public class NotificationServiceImpl extends NotificationService.Impl {
    private static final String TAG = NotificationServiceImpl.class.getSimpleName();

    /**
     * The class which receives the intents from the Android framework. It initializes the
     * Notification service, and forward the intents there. Declared public as it needs to be
     * initialized by the Android framework.
     */
    public static class Receiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.i(TAG, "Received a notification intent in the NotificationService's receiver.");

            // For certain critical notification actions, we might want to perform some processing
            // immediately to reassure the user that the interaction has been acknowledged, without
            // potentially incurring the native startup and job scheduling delay. In some cases,
            // this returns false indicating no further processing necessary.
            if (!NotificationPlatformBridge.dispatchNotificationEventPreNative(intent)) {
                return;
            }

            // Android encourages us not to start services directly on N+, so instead we
            // schedule a job to handle the notification intent. We use the Android JobScheduler
            // rather than GcmNetworkManager or FirebaseJobDispatcher since the JobScheduler
            // allows us to execute immediately by setting an override deadline of zero
            // milliseconds.
            JobScheduler scheduler =
                    (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
            PersistableBundle extras = NotificationJobServiceImpl.getJobExtrasFromIntent(intent);
            putJobScheduledTimeInExtras(extras);

            // Use a different task ID for notification unsubscribe actions to ensure it is not
            // overridden by the task handling the dismiss intent; plus use a higher priority to
            // avoid scheduling delays up to several minutes as indicated by telemetry.
            int taskId = TaskIds.NOTIFICATION_SERVICE_JOB_ID;
            boolean isExpedited = false;
            if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
                taskId = TaskIds.NOTIFICATION_SERVICE_PRE_UNSUBSCRIBE_JOB_ID;
                isExpedited = true;
            }

            JobInfo.Builder jobBuilder =
                    new JobInfo.Builder(
                            taskId, new ComponentName(context, NotificationJobService.class));
            jobBuilder.setExtras(extras);

            // The `setExpedited` option must not be used together with `setDeadlineOverride`, while
            // `setImportantWhileForeground` must be used together with at least one constraint.
            if (isExpedited) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    jobBuilder.setExpedited(true);
                } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    jobBuilder.setImportantWhileForeground(true);
                    jobBuilder.setOverrideDeadline(0);
                } else {
                    assert false; // Not reached, PRE_UNSUBSCRIBE is not supported before Pie.
                }
            } else {
                jobBuilder.setOverrideDeadline(0);
            }

            recordJobIsAlreadyPendingHistogram(scheduler, taskId, intent);
            NotificationUmaTracker.getInstance()
                    .recordIntentHandlerJobStage(
                            NotificationUmaTracker.IntentHandlerJobStage.SCHEDULE_JOB,
                            intent.getAction());

            JobInfo job = jobBuilder.build();
            int result = scheduler.schedule(job);

            if (result != JobScheduler.RESULT_SUCCESS) {
                NotificationUmaTracker.getInstance()
                        .recordIntentHandlerJobStage(
                                NotificationUmaTracker.IntentHandlerJobStage.SCHEDULE_JOB_FAILED,
                                intent.getAction());
            }

            recordJobScheduleResultHistogram(result, intent);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                recordJobPendingReasonHistogram(scheduler, taskId, intent);
            }
        }

        private static void recordJobIsAlreadyPendingHistogram(
                JobScheduler scheduler, int taskId, Intent intent) {
            boolean isAlreadyPending = scheduler.getPendingJob(taskId) != null;
            RecordHistogram.recordBooleanHistogram(
                    "Notifications.Android.JobIsAlreadyPending", isAlreadyPending);
            if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
                RecordHistogram.recordBooleanHistogram(
                        "Notifications.Android.JobIsAlreadyPending.PreUnsubscribe",
                        isAlreadyPending);
            }
        }

        private static void recordJobScheduleResultHistogram(int result, Intent intent) {
            boolean isSuccess = (result == JobScheduler.RESULT_SUCCESS);
            RecordHistogram.recordBooleanHistogram(
                    "Notifications.Android.JobScheduleResult", isSuccess);
            if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
                RecordHistogram.recordBooleanHistogram(
                        "Notifications.Android.JobScheduleResult.PreUnsubscribe", isSuccess);
            }
        }

        @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
        private static void recordJobPendingReasonHistogram(
                JobScheduler scheduler, int taskId, Intent intent) {
            int jobPendingReason = scheduler.getPendingJobReason(taskId);
            RecordHistogram.recordSparseHistogram(
                    "Notifications.Android.JobPendingReason", jobPendingReason);
            if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
                RecordHistogram.recordSparseHistogram(
                        "Notifications.Android.JobPendingReason.PreUnsubscribe", jobPendingReason);
            }
        }

        private static void putJobScheduledTimeInExtras(PersistableBundle extras) {
            extras.putLong(
                    NotificationConstants.EXTRA_JOB_SCHEDULED_TIME_MS,
                    SystemClock.elapsedRealtime());
        }
    }

    /**
     * Called when a Notification has been interacted with by the user. If we can verify that the
     * Intent has a notification Id, start Chrome (if needed) on the UI thread.
     *
     * @param intent The intent containing the specific information.
     */
    @Override
    public void onHandleIntent(final Intent intent) {
        if (!intent.hasExtra(NotificationConstants.EXTRA_NOTIFICATION_ID)
                || !intent.hasExtra(NotificationConstants.EXTRA_NOTIFICATION_INFO_ORIGIN)) {
            return;
        }

        if (NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction())) {
            Receiver receiver = new Receiver();
            receiver.onReceive(ContextUtils.getApplicationContext(), intent);
            return;
        }

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    dispatchIntentOnUIThread(intent);
                });

        PostTask.runOrPostTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    WebappsUtils.prepareIsRequestPinShortcutSupported();
                });
    }

    /**
     * Initializes Chrome and starts the browser process if it's not running as of yet, and dispatch
     * |intent| to the NotificationPlatformBridge once this is done.
     *
     * @param intent The intent containing the notification's information.
     */
    static void dispatchIntentOnUIThread(Intent intent) {
        final BrowserParts parts =
                new EmptyBrowserParts() {
                    @Override
                    public void finishNativeInitialization() {
                        // Warm up the WebappRegistry, as we need to check if this notification
                        // should launch a
                        // standalone web app. This no-ops if the registry is already initialized
                        // and warmed.
                        WebappRegistry.getInstance();
                        WebappRegistry.warmUpSharedPrefs();

                        // Now that the browser process is initialized, we pass forward the call to
                        // the
                        // NotificationPlatformBridge which will take care of delivering the
                        // appropriate events.
                        NotificationUmaTracker.getInstance()
                                .recordIntentHandlerJobStage(
                                        NotificationUmaTracker.IntentHandlerJobStage.DISPATCH_EVENT,
                                        intent.getAction());
                        if (!NotificationPlatformBridge.dispatchNotificationEvent(intent)) {
                            Log.w(TAG, "Unable to dispatch the notification event to Chrome.");
                        }

                        // TODO(peter): Verify that the lifetime of the NotificationService is
                        // sufficient
                        // when a notification event could be dispatched successfully.
                    }
                };

        NotificationUmaTracker.getInstance()
                .recordIntentHandlerJobStage(
                        NotificationUmaTracker.IntentHandlerJobStage.NATIVE_STARTUP,
                        intent.getAction());

        // Try to load native.
        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);

        // TODO(crbug.com/324827395): A blocking start-up ensures that `onStartJob` does not return
        // `false` and does not release the wake lock prematurely. Clean this up once we have
        // confirmation in telemetry that this solution is effective.
        boolean isAsync = !NotificationConstants.ACTION_PRE_UNSUBSCRIBE.equals(intent.getAction());
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(isAsync, parts);
    }
}
