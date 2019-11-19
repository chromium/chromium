// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.component_updater;

import android.os.Build;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Java-side implementation of the component update scheduler using the BackgroundTaskScheduler. */
@JNINamespace("component_updater")
public class UpdateScheduler {
    private static UpdateScheduler sInstance;
    private TaskFinishedCallback mTaskFinishedCallback;
    private long mNativeScheduler;
    private long mDelayMs;

    @CalledByNative
    /* package */ static UpdateScheduler getInstance() {
        if (sInstance == null) {
            sInstance = new UpdateScheduler();
        }
        return sInstance;
    }

    @CalledByNative
    /* package */ static boolean isAvailable() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                || GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(
                           ContextUtils.getApplicationContext())
                == ConnectionResult.SUCCESS;
    }

    /* package */ void onStartTaskBeforeNativeLoaded(TaskFinishedCallback callback) {
        mTaskFinishedCallback = callback;
    }

    /* package */ void onStartTaskWithNative() {
        assert mNativeScheduler != 0;
        UpdateSchedulerJni.get().onStartTask(mNativeScheduler, UpdateScheduler.this);
    }

    /* package */ void onStopTask() {
        if (mNativeScheduler != 0) {
            UpdateSchedulerJni.get().onStopTask(mNativeScheduler, UpdateScheduler.this);
        }
        mTaskFinishedCallback = null;
        scheduleInternal(mDelayMs);
    }

    /* package */ void reschedule() {
        scheduleInternal(mDelayMs);
    }

    private UpdateScheduler() {}

    private void scheduleInternal(long delayMs) {
        // Skip re-scheduling if we are currently running the update task. Otherwise, the current
        // update tasks would be cancelled.
        if (mTaskFinishedCallback != null) return;

        TaskInfo taskInfo = TaskInfo.createOneOffTask(TaskIds.COMPONENT_UPDATE_JOB_ID,
                                            UpdateTask.class, delayMs, Integer.MAX_VALUE)
                                    .setUpdateCurrent(true)
                                    .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                                    .setIsPersisted(true)
                                    .build();
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfo);
    }

    @CalledByNative
    private void schedule(long initialDelayMs, long delayMs) {
        mDelayMs = delayMs;
        scheduleInternal(initialDelayMs);
    }

    @CalledByNative
    private void finishTask(boolean reschedule) {
        assert mTaskFinishedCallback != null;
        mTaskFinishedCallback.taskFinished(false);
        mTaskFinishedCallback = null;
        if (reschedule) {
            scheduleInternal(mDelayMs);
        }
    }

    @CalledByNative
    private void setNativeScheduler(long nativeScheduler) {
        mNativeScheduler = nativeScheduler;
    }

    @CalledByNative
    private void cancelTask() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.COMPONENT_UPDATE_JOB_ID);
    }

    @NativeMethods
    interface Natives {
        void onStartTask(long nativeBackgroundTaskUpdateScheduler, UpdateScheduler caller);
        void onStopTask(long nativeBackgroundTaskUpdateScheduler, UpdateScheduler caller);
    }
}