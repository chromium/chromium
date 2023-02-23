// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.os.PersistableBundle;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * Class responsible for scheduling and canceling offline page related background tasks.
 */
public class BackgroundScheduler {
    static final long NO_DELAY = 0;
    private static final boolean OVERWRITE = true;

    private static class LazyHolder {
        static final BackgroundScheduler INSTANCE = new BackgroundScheduler();
    }

    /** Provides an instance of BackgroundScheduler for given context and current API level. */
    public static BackgroundScheduler getInstance() {
        return LazyHolder.INSTANCE;
    }

    /** Cancels a background tasks. */
    public void cancel() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID);
    }

    /** Schedules a background task for provided triggering conditions. */
    public void schedule(TriggerConditions triggerConditions) {
        scheduleImpl(triggerConditions, NO_DELAY, DateUtils.WEEK_IN_MILLIS, OVERWRITE);
    }

    /**
     * If there is no currently scheduled task, then start a GCM Network Manager request
     * for the given Triggering conditions but delayed to run after {@code delayStartSeconds}.
     * Typically, the Request Coordinator will overwrite this task after task processing
     * and/or queue updates. This is a backup task in case processing is killed by the
     * system.
     */
    public void scheduleBackup(TriggerConditions triggerConditions, long delayStartMs) {
        scheduleImpl(triggerConditions, delayStartMs, DateUtils.WEEK_IN_MILLIS, !OVERWRITE);
    }

    /**
     * Method for rescheduling a background task for offline pages in the event of OS upgrade or
     * GooglePlayServices upgrade.
     * We use the least restrictive trigger conditions.  A wakeup will cause the queue to be
     * checked, and the trigger conditions will be replaced by the current trigger conditions
     * needed.
     */
    public void reschedule() {
        // TODO(crbug.com/1414628): Investigate if this can be deleted.
        TriggerConditions triggerConditions = new TriggerConditions(false, 0, false);
        scheduleBackup(triggerConditions, DateUtils.MINUTE_IN_MILLIS * 5);
    }

    protected void scheduleImpl(TriggerConditions triggerConditions, long delayStartMs,
            long executionDeadlineMs, boolean overwrite) {
        PersistableBundle taskExtras = new PersistableBundle();
        TaskExtrasPacker.packTimeInBundle(taskExtras);
        TaskExtrasPacker.packTriggerConditionsInBundle(taskExtras, triggerConditions);

        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, delayStartMs,
                                executionDeadlineMs)
                        .setRequiredNetworkType(triggerConditions.requireUnmeteredNetwork()
                                        ? TaskInfo.NetworkType.UNMETERED
                                        : TaskInfo.NetworkType.ANY)
                        .setUpdateCurrent(overwrite)
                        .setIsPersisted(true)
                        .setExtras(taskExtras)
                        .setRequiresCharging(triggerConditions.requirePowerConnected())
                        .build();

        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfo);
    }
}
