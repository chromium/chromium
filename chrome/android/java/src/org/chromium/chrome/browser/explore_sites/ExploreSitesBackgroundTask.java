// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask.StartBeforeNativeResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

/**
 * Schedules and executes background update of the Explore Sites catalog.
 *
 * The catalog is updated on a somewhat slow schedule, but this code checks for updates
 * approximately daily so that freshness is maintained.
 */
@JNINamespace("explore_sites")
@UsedByReflection("BackgroundTaskReflection.java")
public class ExploreSitesBackgroundTask extends NativeBackgroundTask {
    private static final String TAG = "ESBackgroundTask";
    public static final int DEFAULT_DELAY_HOURS = 25;
    public static final int DEFAULT_FLEX_HOURS = 2;

    private TaskFinishedCallback mTaskFinishedCallback;
    private Profile mProfile;

    public ExploreSitesBackgroundTask() {}

    @VisibleForTesting
    protected Profile getProfile() {
        if (mProfile == null) mProfile = Profile.getLastUsedProfile();
        return mProfile;
    }

    @Override
    public @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        @ConnectionType
        int connectionType = DeviceConditions.getCurrentNetConnectionType(context);
        if (connectionType == ConnectionType.CONNECTION_NONE) {
            return StartBeforeNativeResult.RESCHEDULE;
        }
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.EXPLORE_SITES_REFRESH_JOB_ID;
        if (!ExploreSitesBridge.isEnabled(ExploreSitesBridge.getVariation())) {
            cancelTask();
            return;
        }

        mTaskFinishedCallback = callback;
        ExploreSitesBridge.updateCatalogFromNetwork(getProfile(), false /*isImmediateFetch*/,
                (ignored) -> mTaskFinishedCallback.taskFinished(false));
        RecordHistogram.recordEnumeratedHistogram("ExploreSites.CatalogUpdateRequestSource",
                ExploreSitesCatalogUpdateRequestSource.BACKGROUND,
                ExploreSitesCatalogUpdateRequestSource.NUM_ENTRIES);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        // Don't retry, this task is periodic anyways.
        return false;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        // Don't retry, this task is periodic anyways.
        return false;
    }

    @Override
    public void reschedule(Context context) {
        schedule(true /* updateCurrent */);
    }

    // Removes the task from the JobScheduler queue.  Should be called when the feature is disabled.
    public static void cancelTask() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.EXPLORE_SITES_REFRESH_JOB_ID);
    }

    /**
     * Cancels an obsolete task ID that was saved in JobScheduler with the wrong background task
     * class name.
     */
    public static void cancelObsoleteTaskId() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(ContextUtils.getApplicationContext(),
                TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID);
    }

    // Begins the periodic update task.
    public static void schedule(boolean updateCurrent) {
        cancelObsoleteTaskId();
        TaskInfo.Builder taskInfoBuilder =
                TaskInfo.createPeriodicTask(TaskIds.EXPLORE_SITES_REFRESH_JOB_ID,
                                ExploreSitesBackgroundTask.class,
                                DateUtils.HOUR_IN_MILLIS * DEFAULT_DELAY_HOURS,
                                DateUtils.HOUR_IN_MILLIS * DEFAULT_FLEX_HOURS)
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setIsPersisted(true)
                        .setUpdateCurrent(updateCurrent);
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfoBuilder.build());
    }
}
