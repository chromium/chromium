// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.annotation.TargetApi;
import android.app.IntentService;
import android.app.job.JobService;
import android.content.Context;
import android.os.Build;
import android.support.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Manages scheduling and running of the Omaha client code.
 * Delegates out to either an {@link IntentService} or {@link JobService}, as necessary.
 */
public class OmahaService extends OmahaBase implements BackgroundTask {
    private static class OmahaClientDelegate extends OmahaDelegateBase {
        public OmahaClientDelegate(Context context) {
            super(context);
        }

        @Override
        public void scheduleService(long currentTimestampMs, long nextTimestampMs) {
            if (Build.VERSION.SDK_INT < OmahaBase.MIN_API_JOB_SCHEDULER) {
                getScheduler().createAlarm(OmahaClient.createIntent(getContext()), nextTimestampMs);
                Log.i(OmahaBase.TAG, "Scheduled using AlarmManager and IntentService");
            } else {
                final long delay = nextTimestampMs - currentTimestampMs;
                ThreadUtils.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (scheduleJobService(getContext(), delay)) {
                            Log.i(OmahaBase.TAG, "Scheduled using JobService");
                        } else {
                            Log.e(OmahaBase.TAG, "Failed to schedule job");
                        }
                    }
                });
            }
        }
    }

    private static final Object DELEGATE_LOCK = new Object();
    private static OmahaService sInstance;

    @Nullable
    public static OmahaService getInstance(Context context) {
        synchronized (DELEGATE_LOCK) {
            if (sInstance == null) sInstance = new OmahaService(context);
            return sInstance;
        }
    }

    private AsyncTask<Void> mJobServiceTask;

    /** Used only by {@link BackgroundTaskScheduler}. */
    public OmahaService() {
        this(ContextUtils.getApplicationContext());
    }

    private OmahaService(Context context) {
        super(new OmahaClientDelegate(context));
    }

    /**
     * Trigger the {@link BackgroundTaskScheduler} immediately.
     * Must only be called by {@link OmahaBase#onForegroundSessionStart}.
     */
    static void startServiceImmediately(Context context) {
        if (Build.VERSION.SDK_INT < OmahaBase.MIN_API_JOB_SCHEDULER) {
            context.startService(OmahaClient.createIntent(context));
        } else {
            scheduleJobService(context, 0);
        }
    }

    // Incorrectly infers that this is called on a worker thread because of AsyncTask doInBackground
    // overriding.
    @SuppressWarnings("WrongThread")
    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public boolean onStartTask(
            Context context, TaskParameters parameters, final TaskFinishedCallback callback) {
        mJobServiceTask = new AsyncTask<Void>() {
            @Override
            public Void doInBackground() {
                run();
                return null;
            }

            @Override
            public void onPostExecute(Void result) {
                callback.taskFinished(false);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        return false;
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        if (mJobServiceTask != null) {
            mJobServiceTask.cancel(false);
            mJobServiceTask = null;
        }
        return false;
    }

    @Override
    @TargetApi(Build.VERSION_CODES.M)
    public void reschedule(Context context) {
        // Needs appropriate implementation.
    }

    /**
     * Schedules the Omaha code to run at the given time.
     * @param context Context to use.
     * @param delayMs How long to wait until the job should be triggered.
     */
    @TargetApi(Build.VERSION_CODES.M)
    static boolean scheduleJobService(Context context, long delayMs) {
        long latency = Math.max(0, delayMs);

        TaskInfo taskInfo = TaskInfo.createOneOffTask(TaskIds.OMAHA_JOB_ID, OmahaService.class,
                                            latency, latency)
                                    .build();
        return BackgroundTaskSchedulerFactory.getScheduler().schedule(context, taskInfo);
    }
}
