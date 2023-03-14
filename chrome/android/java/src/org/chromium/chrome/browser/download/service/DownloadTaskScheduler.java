// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import android.os.PersistableBundle;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
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

    @CalledByNative
    private static void scheduleTask(@DownloadTaskType int taskType,
            boolean requiresUnmeteredNetwork, boolean requiresCharging,
            int optimalBatteryPercentage, long windowStartTimeSeconds, long windowEndTimeSeconds) {
        PersistableBundle bundle = new PersistableBundle();
        bundle.putInt(EXTRA_TASK_TYPE, taskType);
        bundle.putInt(EXTRA_OPTIMAL_BATTERY_PERCENTAGE, optimalBatteryPercentage);
        bundle.putBoolean(EXTRA_BATTERY_REQUIRES_CHARGING, requiresCharging);

        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        TaskInfo taskInfo = TaskInfo.createOneOffTask(getTaskId(taskType),
                                            DateUtils.SECOND_IN_MILLIS * windowStartTimeSeconds,
                                            DateUtils.SECOND_IN_MILLIS * windowEndTimeSeconds)
                                    .setRequiredNetworkType(getRequiredNetworkType(
                                            taskType, requiresUnmeteredNetwork))
                                    .setRequiresCharging(requiresCharging)
                                    .setUpdateCurrent(true)
                                    .setIsPersisted(true)
                                    .setExtras(bundle)
                                    .build();
        scheduler.schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    @CalledByNative
    private static void cancelTask(@DownloadTaskType int taskType) {
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
                return requiresUnmeteredNetwork ? TaskInfo.NetworkType.UNMETERED
                                                : TaskInfo.NetworkType.ANY;
            case DownloadTaskType.DOWNLOAD_LATER_TASK:
                return TaskInfo.NetworkType.ANY;
        }
        assert false : "Unknown download task type.";
        return -1;
    }
}
