// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import android.os.PersistableBundle;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskInfo.OneOffInfo;
import org.chromium.components.download.DownloadTaskType;

/**
 * A background task scheduler that schedules various types of jobs with the system with certain
 * conditions as requested by the download service.
 */
@JNINamespace("download::android")
public class DownloadTaskScheduler {
    public static final String EXTRA_BATTERY_REQUIRES_CHARGING = "extra_battery_requires_charging";
    public static final String EXTRA_OPTIMAL_BATTERY_PERCENTAGE =
            "extra_optimal_battery_percentage";
    public static final String EXTRA_TASK_TYPE = "extra_task_type";

    @VisibleForTesting
    @CalledByNative
    public static void scheduleTask(
            @DownloadTaskType int taskType,
            boolean requiresUnmeteredNetwork,
            boolean requiresCharging,
            int optimalBatteryPercentage,
            long windowStartTimeSeconds,
            long windowEndTimeSeconds) {
        PersistableBundle bundle = new PersistableBundle();
        bundle.putInt(EXTRA_TASK_TYPE, taskType);
        bundle.putInt(EXTRA_OPTIMAL_BATTERY_PERCENTAGE, optimalBatteryPercentage);
        bundle.putBoolean(EXTRA_BATTERY_REQUIRES_CHARGING, requiresCharging);

        int taskId = getTaskId(taskType);
        boolean isUserInitiatedJob = DownloadUtils.isUserInitiatedJob(taskId);

        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        OneOffInfo.Builder oneOffInfoBuilder = new OneOffInfo.Builder();
        if (!isUserInitiatedJob) {
            oneOffInfoBuilder.setWindowStartTimeMs(
                    DateUtils.SECOND_IN_MILLIS * windowStartTimeSeconds);
            oneOffInfoBuilder.setWindowEndTimeMs(DateUtils.SECOND_IN_MILLIS * windowEndTimeSeconds);
        }

        TaskInfo.Builder builder = TaskInfo.createTask(taskId, oneOffInfoBuilder.build());
        builder.setRequiredNetworkType(getRequiredNetworkType(taskType, requiresUnmeteredNetwork))
                .setRequiresCharging(requiresCharging)
                .setUserInitiated(isUserInitiatedJob)
                .setUpdateCurrent(true)
                .setIsPersisted(true)
                .setExtras(bundle);

        scheduler.schedule(ContextUtils.getApplicationContext(), builder.build());
    }

    @CalledByNative
    public static void cancelTask(@DownloadTaskType int taskType) {
        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        scheduler.cancel(ContextUtils.getApplicationContext(), getTaskId(taskType));
    }

    private static int getTaskId(@DownloadTaskType int taskType) {
        switch (taskType) {
            case DownloadTaskType.DOWNLOAD_TASK:
                return TaskIds.DOWNLOAD_SERVICE_JOB_ID;
            case DownloadTaskType.CLEANUP_TASK:
                return TaskIds.DOWNLOAD_CLEANUP_JOB_ID;
            case DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK:
                return TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID;
            case DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK:
                return TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID;
            case DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK:
                return TaskIds.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_JOB_ID;
            case DownloadTaskType.DOWNLOAD_LATER_TASK:
                return TaskIds.DOWNLOAD_LATER_JOB_ID;
        }
        assert false : "Unknown download task type.";
        return -1;
    }

    private static int getRequiredNetworkType(
            @DownloadTaskType int taskType, boolean requiresUnmeteredNetwork) {
        switch (taskType) {
            case DownloadTaskType.CLEANUP_TASK:
                return TaskInfo.NetworkType.NONE;
            case DownloadTaskType.DOWNLOAD_TASK: // intentional fall-through
            case DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK:
            case DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_TASK:
            case DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_TASK:
                return requiresUnmeteredNetwork
                        ? TaskInfo.NetworkType.UNMETERED
                        : TaskInfo.NetworkType.ANY;
            case DownloadTaskType.DOWNLOAD_LATER_TASK:
                return TaskInfo.NetworkType.ANY;
        }
        assert false : "Unknown download task type.";
        return -1;
    }
}
