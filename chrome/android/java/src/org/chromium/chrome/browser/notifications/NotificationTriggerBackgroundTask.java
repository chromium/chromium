// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Handles notification triggers scheduled via {@link NotificationTriggerScheduler}.
 * This task calls NotificationTriggerScheduler::triggerNotifications after loading native code.
 */
public class NotificationTriggerBackgroundTask extends NativeBackgroundTask {
    @VisibleForTesting protected static final String KEY_TIMESTAMP = "Timestamp";

    /** Indicates whether we should reschedule this task if it gets stopped. */
    private boolean mShouldReschedule = true;

    @Override
    public @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.NOTIFICATION_TRIGGER_JOB_ID;
        // Check if we need to continue by waking up native or this trigger got handled already.
        mShouldReschedule =
                NotificationTriggerScheduler.getInstance()
                        .checkAndResetTrigger(taskParameters.getExtras().getLong(KEY_TIMESTAMP));
        return mShouldReschedule
                ? StartBeforeNativeResult.LOAD_NATIVE
                : StartBeforeNativeResult.DONE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.NOTIFICATION_TRIGGER_JOB_ID;
        // Simply waking up native should have triggered all outstanding notifications already.
        // Explicitly calling TriggerNotifications here in case Chrome was already running.
        NotificationTriggerScheduler.getInstance().triggerNotifications();
        mShouldReschedule = false;
        callback.taskFinished(false);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.NOTIFICATION_TRIGGER_JOB_ID;
        return mShouldReschedule;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.NOTIFICATION_TRIGGER_JOB_ID;
        return mShouldReschedule;
    }

    /**
     * Cancels any pending tasks with this ID. Note that a task that has already started executing
     * might still continue to run after this has been called.
     */
    public static void cancel() {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), TaskIds.NOTIFICATION_TRIGGER_JOB_ID);
    }
}
