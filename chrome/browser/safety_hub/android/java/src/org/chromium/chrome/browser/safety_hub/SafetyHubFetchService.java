// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.concurrent.TimeUnit;

/** Manages the scheduling of Safety Hub fetch jobs. */
public class SafetyHubFetchService implements BackgroundTask {
    private static final int SAFETY_HUB_JOB_INTERVAL_IN_DAYS = 1;
    private static final int SAFETY_HUB_JOB_FLEX_IN_MINUTES = 15;

    /** Used only by {@link BackgroundTaskScheduler}. */
    public SafetyHubFetchService() {}

    /** See {@link ChromeActivitySessionTracker#onForegroundSessionStart()}. */
    public static void onForegroundSessionStart() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFETY_HUB)) {
            schedulePeriodicFetchJob();
        } else {
            cancelPeriodicFetchJob();
        }
    }

    /** Schedules the fetch job to run periodically at the given interval. */
    private static void schedulePeriodicFetchJob() {
        TaskInfo.TimingInfo periodicTimingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TimeUnit.DAYS.toMillis(SAFETY_HUB_JOB_INTERVAL_IN_DAYS))
                        .setFlexMs(TimeUnit.MINUTES.toMillis(SAFETY_HUB_JOB_FLEX_IN_MINUTES))
                        .build();

        TaskInfo taskInfo =
                TaskInfo.createTask(TaskIds.SAFETY_HUB_JOB_ID, periodicTimingInfo)
                        .setUpdateCurrent(false)
                        .setIsPersisted(true)
                        .build();

        BackgroundTaskSchedulerFactory.getScheduler()
                .schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    private static void cancelPeriodicFetchJob() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.SAFETY_HUB_JOB_ID);
    }

    @Override
    public boolean onStartTask(
            Context context, TaskParameters parameters, final TaskFinishedCallback callback) {
        fetchBreachedCredentialsCount(callback);
        return false;
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        return true;
    }

    private void fetchBreachedCredentialsCount(final TaskFinishedCallback callback) {
        // TODO(crbug.com/324562205): Implement the fetch calls to GMSCore.
        callback.taskFinished(false);
    }
}
