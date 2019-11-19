// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.TimeUnit;

/**
 * A task implementation that loads native and then tries to refresh the Feed's articles. Failures
 * or interruptions are not retried or rescheduled.
 */
public class FeedRefreshTask extends NativeBackgroundTask {
    // The amount of "flex" to add around the fetching periods, as a ratio of the period.
    private static final double FLEX_FACTOR = 0.1;

    /**
     * Schedules a periodic task to wake up every thresholdMs and try to refresh. Allows for a bit
     * of flex time before and after thresholdMs. Replaces the previous scheduling of the feed
     * refresh task.
     *
     * @param thresholdMs the target number of milliseconds between each refresh.
     */
    public static void scheduleWakeUp(long thresholdMs) {
        // The flex given to the BackgroundTaskScheduler is a period of time before the |intervalMs|
        // point in time. Because we want to schedule for +/- FLEX_FACTOR around |thresholdMs|, some
        // adjustments below are needed.
        long intervalMs = (long) (thresholdMs * (1 + FLEX_FACTOR));
        long flexWindowSizeMs = (long) (thresholdMs * 2 * FLEX_FACTOR);

        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        TaskInfo taskInfo = TaskInfo.createPeriodicTask(TaskIds.FEED_REFRESH_JOB_ID,
                                            FeedRefreshTask.class, intervalMs, flexWindowSizeMs)
                                    .setIsPersisted(true)
                                    .setUpdateCurrent(true)
                                    .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                                    .build();
        scheduler.schedule(ContextUtils.getApplicationContext(), taskInfo);
    }

    /** Clears previously scheduled task. */
    public static void cancelWakeUp() {
        BackgroundTaskScheduler scheduler = BackgroundTaskSchedulerFactory.getScheduler();
        scheduler.cancel(ContextUtils.getApplicationContext(), TaskIds.FEED_REFRESH_JOB_ID);
    }

    @Override
    protected @NativeBackgroundTask.StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        // Nothing to setup without native, just wait.
        return NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        FeedScheduler scheduler = FeedProcessScopeFactory.getFeedScheduler();
        if (scheduler != null) {
            scheduler.onFixedTimer(() -> {
                // Regardless of success, mark ourselves as completed.
                callback.taskFinished(false);
            });
        } else {
            // If the FeedProcessScopeFactory is vending nulls, the Feed is disabled.
            cancelWakeUp();
        }
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
        // The user may be trying to do something other than use Chrome right now, so we want to be
        // as light weight as possible. Instead of going to native to reschedule with a more
        // informed threshold, just schedule for 1 day from now. The next time a successful fetch
        // occurs, this value will be set to a value tailored to current usage patterns.
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> scheduleWakeUp(TimeUnit.DAYS.toMillis(1)));
    }
}
