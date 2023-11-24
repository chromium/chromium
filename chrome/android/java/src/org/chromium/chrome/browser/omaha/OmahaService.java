// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.app.IntentService;
import android.app.job.JobService;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
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
        @Override
        public void scheduleService(long currentTimestampMs, long nextTimestampMs) {
            final long delay = nextTimestampMs - currentTimestampMs;
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (scheduleJobService(delay)) {
                            Log.i(OmahaBase.TAG, "Scheduled using JobService");
                        } else {
                            Log.e(OmahaBase.TAG, "Failed to schedule job");
                        }
                    });
        }
    }

    private static final Object DELEGATE_LOCK = new Object();
    private static OmahaService sInstance;
    private static boolean sHasPendingJob;

    public static @Nullable OmahaService getInstance() {
        synchronized (DELEGATE_LOCK) {
            if (sInstance == null) sInstance = new OmahaService();
            return sInstance;
        }
    }

    private AsyncTask<Void> mJobServiceTask;

    /** Used only by {@link BackgroundTaskScheduler}. */
    public OmahaService() {
        super(new OmahaClientDelegate());
    }

    /**
     * Trigger the {@link BackgroundTaskScheduler} immediately.
     * Must only be called by {@link OmahaBase#onForegroundSessionStart}.
     */
    static void startServiceImmediately() {
        if (sHasPendingJob) return;
        scheduleJobService(0);
    }

    // Incorrectly infers that this is called on a worker thread because of AsyncTask doInBackground
    // overriding.
    @SuppressWarnings("WrongThread")
    @Override
    public boolean onStartTask(
            Context context, TaskParameters parameters, final TaskFinishedCallback callback) {
        sHasPendingJob = false;
        mJobServiceTask =
                new AsyncTask<Void>() {
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
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // Just in case it's possible for onStopTask to be called before onStartTask, we should
        // clear this flag to avoid getting stuck in state where we won't ever be scheduled again.
        sHasPendingJob = false;
        if (mJobServiceTask != null) {
            mJobServiceTask.cancel(false);
            mJobServiceTask = null;
        }
        return false;
    }

    /**
     * Schedules the Omaha code to run at the given time.
     * @param delayMs How long to wait until the job should be triggered.
     */
    static boolean scheduleJobService(long delayMs) {
        long latency = Math.max(0, delayMs);

        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(TaskIds.OMAHA_JOB_ID, latency, latency).build();
        sHasPendingJob =
                BackgroundTaskSchedulerFactory.getScheduler()
                        .schedule(ContextUtils.getApplicationContext(), taskInfo);
        return sHasPendingJob;
    }
}
