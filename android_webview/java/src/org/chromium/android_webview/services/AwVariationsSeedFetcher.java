// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.annotation.TargetApi;
import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.os.Build;

import org.chromium.android_webview.VariationsUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.io.IOException;
import java.util.Date;
import java.util.concurrent.TimeUnit;

/**
 * AwVariationsSeedFetcher is a JobService which periodically downloads the variations seed. We use
 * JobService instead of BackgroundTaskScheduler, since JobService is available on L+, and WebView
 * is L+ only. The job is scheduled whenever an app requests the seed, and it's been at least 1 day
 * since the last fetch. If WebView is never used, the job will never run. The 1-day minimum fetch
 * period is chosen as a trade-off between seed freshness (and prompt delivery of feature
 * killswitches) and data and battery usage. Various Android versions may enforce longer periods,
 * depending on WebView usage and battery-saving features. AwVariationsSeedFetcher is not meant to
 * be used outside the variations service. For the equivalent fetch in Chrome, see
 * AsyncInitTaskRunner$FetchSeedTask.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP) // for JobService
public class AwVariationsSeedFetcher extends JobService {
    private static final String TAG = "AwVariationsSeedFet-";
    private static final int JOB_ID = TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID;
    private static final long MIN_JOB_PERIOD_MILLIS = TimeUnit.DAYS.toMillis(1);

    private static JobScheduler sMockJobScheduler;
    private static VariationsSeedFetcher sMockDownloader;

    private VariationsSeedHolder mSeedHolder;
    private FetchTask mFetchTask;

    private static String getChannelStr() {
        switch (VersionConstants.CHANNEL) {
            case Channel.STABLE: return "stable";
            case Channel.BETA:   return "beta";
            case Channel.DEV:    return "dev";
            case Channel.CANARY: return "canary";
            default: return null; // This is the case for stand-alone WebView.
        }
    }

    // Use JobScheduler.getPendingJob() if it's available. Otherwise, fall back to iterating over
    // all jobs to find the one we want.
    private static JobInfo getPendingJob(JobScheduler scheduler, int jobId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            for (JobInfo info : scheduler.getAllPendingJobs()) {
                if (info.getId() == jobId) return info;
            }
            return null;
        }
        return ApiHelperForN.getPendingJob(scheduler, jobId);
    }

    private static JobScheduler getScheduler() {
        if (sMockJobScheduler != null) return sMockJobScheduler;
        return (JobScheduler) ContextUtils.getApplicationContext().getSystemService(
                Context.JOB_SCHEDULER_SERVICE);
    }

    public static void scheduleIfNeeded() {
        JobScheduler scheduler = getScheduler();

        // Check if it's already scheduled.
        if (getPendingJob(scheduler, JOB_ID) != null) {
            return;
        }

        // Check how long it's been since FetchTask last ran.
        long lastRequestTime = VariationsUtils.getStampTime();
        if (lastRequestTime != 0) {
            long now = (new Date()).getTime();
            if (now < lastRequestTime + MIN_JOB_PERIOD_MILLIS) {
                return;
            }
        }

        ComponentName thisComponent = new ComponentName(
                ContextUtils.getApplicationContext(), AwVariationsSeedFetcher.class);
        JobInfo job = new JobInfo.Builder(JOB_ID, thisComponent)
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setRequiresCharging(true)
                .build();
        if (scheduler.schedule(job) != JobScheduler.RESULT_SUCCESS) {
            Log.e(TAG, "Failed to schedule job");
        }
    }

    private class FetchTask extends AsyncTask<Void> {
        private JobParameters mParams;

        FetchTask(JobParameters params) {
            mParams = params;
        }

        @Override
        protected Void doInBackground() {
            // Should we call jobFinished at the end of this task?
            boolean shouldFinish = true;

            try {
                VariationsUtils.updateStampTime();

                VariationsSeedFetcher downloader =
                        sMockDownloader != null ? sMockDownloader : VariationsSeedFetcher.get();
                String milestone = String.valueOf(VersionConstants.PRODUCT_MAJOR_VERSION);
                SeedInfo newSeed = downloader.downloadContent(
                        VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW,
                        /*restrictMode=*/null, milestone, getChannelStr());

                if (isCancelled()) {
                    return null;
                }

                if (newSeed != null) {
                    mSeedHolder.updateSeed(newSeed, /*onFinished=*/() -> jobFinished(mParams));
                    shouldFinish = false; // jobFinished will be deferred until updateSeed is done.
                }
            } catch (IOException e) {
                // downloadContent() logs and re-throws IOExceptions, so there's no need to log
                // here. IOException includes InterruptedIOException, which may happen inside
                // downloadContent() if the task is cancelled.
            } finally {
                if (shouldFinish) jobFinished(mParams);
            }

            return null;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        ServiceInit.init(getApplicationContext());
        mSeedHolder = VariationsSeedHolder.getInstance();
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        // If this process has survived since the last run of this job, mFetchTask could still
        // exist. Either way, (re)create it with the new params.
        mFetchTask = new FetchTask(params);
        mFetchTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        if (mFetchTask != null) {
            mFetchTask.cancel(true);
            mFetchTask = null;
        }
        return false;
    }

    protected void jobFinished(JobParameters params) {
        assert params.getJobId() == JOB_ID;
        jobFinished(params, /*needsReschedule=*/false);
    }

    public static void setMocks(JobScheduler scheduler, VariationsSeedFetcher fetcher) {
        sMockJobScheduler = scheduler;
        sMockDownloader = fetcher;
    }
}
