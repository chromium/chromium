// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.os.PersistableBundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedFetchInfo;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.net.HttpURLConnection;
import java.util.concurrent.TimeUnit;

/**
 * AwVariationsSeedFetcher is a JobService which periodically downloads the variations seed.
 * The job is scheduled whenever an app requests the seed, and it's been at least 1 day
 * since the last fetch. If WebView is never used, the job will never run. The 1-day minimum fetch
 * period is chosen as a trade-off between seed freshness (and prompt delivery of feature
 * killswitches) and data and battery usage. Various Android versions may enforce longer periods,
 * depending on WebView usage and battery-saving features. AwVariationsSeedFetcher is not meant to
 * be used outside the variations service. For the equivalent fetch in Chrome, see
 * AsyncInitTaskRunner$FetchSeedTask.
 */
// TODO(https://crbug.com/1328637): consider using BackgroundTaskScheduler instead of JobService
public class AwVariationsSeedFetcher extends JobService {
    @VisibleForTesting
    public static final String JOB_REQUEST_COUNT_KEY = "RequestCount";
    @VisibleForTesting
    public static final int JOB_MAX_REQUEST_COUNT = 5;

    private static final String TAG = "AwVariationsSeedFet-";
    private static final int JOB_ID = TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID;
    private static final long MIN_JOB_PERIOD_MILLIS = TimeUnit.HOURS.toMillis(12);
    private static final int JOB_BACKOFF_POLICY = JobInfo.BACKOFF_POLICY_EXPONENTIAL;
    private static final long JOB_INITIAL_BACKOFF_TIME_IN_MS = TimeUnit.MINUTES.toMillis(5);

    /** Clock used to fake time in tests. */
    public interface Clock { long currentTimeMillis(); }

    private static JobScheduler sMockJobScheduler;
    private static VariationsSeedFetcher sMockDownloader;
    private static Clock sTestClock;

    private FetchTask mFetchTask;

    private static long currentTimeMillis() {
        if (sTestClock != null) {
            return sTestClock.currentTimeMillis();
        }
        return System.currentTimeMillis();
    }

    private static String getChannelStr() {
        switch (VersionConstants.CHANNEL) {
            case Channel.STABLE: return "stable";
            case Channel.BETA:   return "beta";
            case Channel.DEV:    return "dev";
            case Channel.CANARY: return "canary";
            default: return null;
        }
    }

    private static JobInfo getPendingJob(JobScheduler scheduler, int jobId) {
        return scheduler.getPendingJob(jobId);
    }

    private static JobScheduler getScheduler() {
        if (sMockJobScheduler != null) return sMockJobScheduler;

        // This may be null due to vendor framework bugs. https://crbug.com/968636
        return (JobScheduler) ContextUtils.getApplicationContext().getSystemService(
                Context.JOB_SCHEDULER_SERVICE);
    }

    public static void scheduleIfNeeded() {
        JobScheduler scheduler = getScheduler();
        if (scheduler == null) return;

        // Check if it's already scheduled.
        if (!CommandLine.getInstance().hasSwitch(AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD)
                && getPendingJob(scheduler, JOB_ID) != null) {
            VariationsUtils.debugLog("Seed download job already scheduled");
            return;
        }

        // Check how long it's been since FetchTask last ran.
        long lastRequestTime = VariationsUtils.getStampTime();
        if (lastRequestTime != 0) {
            long now = currentTimeMillis();
            long minJobPeriodMillis = VariationsUtils.getDurationSwitchValueInMillis(
                    AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD, MIN_JOB_PERIOD_MILLIS);
            if (now < lastRequestTime + minJobPeriodMillis) {
                VariationsUtils.debugLog("Throttling seed download job");
                return;
            }
        }

        VariationsUtils.debugLog("Scheduling seed download job");
        Context context = ContextUtils.getApplicationContext();
        ComponentName thisComponent = new ComponentName(context, AwVariationsSeedFetcher.class);
        PersistableBundle extras = new PersistableBundle(/*capacity=*/1);
        extras.putInt(JOB_REQUEST_COUNT_KEY, 0);
        boolean requiresCharging =
                !CommandLine.getInstance().hasSwitch(AwSwitches.FINCH_SEED_NO_CHARGING_REQUIREMENT);
        JobInfo job =
                new JobInfo.Builder(JOB_ID, thisComponent)
                        .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .setRequiresCharging(requiresCharging)
                        .setBackoffCriteria(JOB_INITIAL_BACKOFF_TIME_IN_MS, JOB_BACKOFF_POLICY)
                        .setExtras(extras)
                        .build();
        if (scheduler.schedule(job) == JobScheduler.RESULT_SUCCESS) {
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
            metrics.setLastEnqueueTime(currentTimeMillis());
            if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
                Log.e(TAG, "Failed to write variations SharedPreferences to disk");
            }
        } else {
            Log.e(TAG, "Failed to schedule job");
        }
    }

    private class FetchTask extends BackgroundOnlyAsyncTask<Void> {
        private JobParameters mParams;

        FetchTask(JobParameters params) {
            mParams = params;
        }

        @Override
        protected Void doInBackground() {
            // Should we call onFinished at the end of this task?
            boolean shouldFinish = true;
            // Should we retry the job?
            boolean needsReschedule = false;
            long startTime = currentTimeMillis();

            try {
                VariationsUtils.updateStampTime();
                SeedInfo info = VariationsUtils.readSeedFile(VariationsUtils.getSeedFile());
                VariationsUtils.debugLog("Downloading new seed");
                VariationsSeedFetcher downloader =
                        sMockDownloader != null ? sMockDownloader : VariationsSeedFetcher.get();
                String milestone = String.valueOf(VersionConstants.PRODUCT_MAJOR_VERSION);
                SeedFetchInfo fetchInfo = downloader.downloadContent(
                        VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW,
                        /*restrictMode=*/null, milestone, getChannelStr(),
                        /*currentSeedInfo=*/info);

                saveMetrics(startTime, /*endTime=*/currentTimeMillis());

                if (isCancelled()) {
                    return null;
                }

                // VariationsSeedFetcher returns HttpURLConnection.HTTP_NOT_MODIFIED if seed did not
                // change server-side, or HttpURLConnection.HTTP_OK if a new seed was successfully
                // fetched
                if (HttpURLConnection.HTTP_OK != fetchInfo.seedFetchResult
                        && HttpURLConnection.HTTP_NOT_MODIFIED != fetchInfo.seedFetchResult) {
                    int requestCount = mParams.getExtras().getInt(JOB_REQUEST_COUNT_KEY) + 1;
                    mParams.getExtras().putInt(JOB_REQUEST_COUNT_KEY, requestCount);
                    // Limit the retries to JOB_MAX_REQUEST_COUNT.
                    needsReschedule = (requestCount <= JOB_MAX_REQUEST_COUNT);
                }
                if (fetchInfo.seedInfo != null) {
                    VariationsSeedHolder.getInstance().updateSeed(fetchInfo.seedInfo,
                            /*onFinished=*/() -> onFinished(mParams, /*needsReschedule=*/false));
                    shouldFinish = false; // jobFinished will be deferred until updateSeed is done.
                }
            } finally {
                if (shouldFinish) onFinished(mParams, needsReschedule);
            }

            return null;
        }

        private void saveMetrics(long startTime, long endTime) {
            Context context = ContextUtils.getApplicationContext();
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
            if (metrics.hasLastEnqueueTime()) {
                metrics.setJobQueueTime(startTime - metrics.getLastEnqueueTime());
            }
            if (metrics.hasLastJobStartTime()) {
                metrics.setJobInterval(startTime - metrics.getLastJobStartTime());
            }
            metrics.clearLastEnqueueTime();
            metrics.setLastJobStartTime(startTime);
            if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
                Log.e(TAG, "Failed to write variations SharedPreferences to disk");
            }
        }
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

    protected void onFinished(JobParameters params, boolean needsReschedule) {
        assert params.getJobId() == JOB_ID;
        jobFinished(params, needsReschedule);
    }

    public static void setMocks(JobScheduler scheduler, VariationsSeedFetcher fetcher) {
        sMockJobScheduler = scheduler;
        sMockDownloader = fetcher;
    }

    public static void setTestClock(Clock clock) {
        sTestClock = clock;
    }
}
