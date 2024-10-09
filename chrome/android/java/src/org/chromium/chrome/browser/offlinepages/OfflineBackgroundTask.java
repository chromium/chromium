// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.os.PersistableBundle;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/**
 * Handles servicing of background offlining requests coming via background_task_scheduler
 * component.
 */
public class OfflineBackgroundTask extends NativeBackgroundTask {
    private static final String TAG = "OfflineBkgrndTask";

    @Override
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;

        if (!checkConditions(context, taskParameters.getExtras())) {
            return StartBeforeNativeResult.RESCHEDULE;
        }

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;

        if (!startScheduledProcessing(
                BackgroundSchedulerProcessor.getInstance(),
                context,
                taskParameters.getExtras(),
                wrapCallback(callback))) {
            callback.taskFinished(true);
            return;
        }

        // Set up backup scheduled task in case processing is killed before RequestCoordinator
        // has a chance to reschedule base on remaining work.
        BackgroundScheduler.getInstance()
                .scheduleBackup(
                        TaskExtrasPacker.unpackTriggerConditionsFromBundle(
                                taskParameters.getExtras()),
                        DateUtils.MINUTE_IN_MILLIS * 5);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;

        // Native didn't complete loading, but it was supposed to. Presume we need to reschedule.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID;

        return BackgroundSchedulerProcessor.getInstance().stopScheduledProcessing();
    }

    /** Wraps the callback for code reuse */
    private Callback<Boolean> wrapCallback(final TaskFinishedCallback callback) {
        return new Callback<Boolean>() {
            @Override
            public void onResult(Boolean result) {
                callback.taskFinished(result);
            }
        };
    }

    /**
     * Starts scheduled processing and reports UMA. This method does not check for current
     * conditions and should be used together with {@link #checkConditions} to ensure that it
     * performs the tasks only when it is supposed to.
     *
     * @return Whether processing will be carried out and completion will be indicated through a
     *     callback.
     */
    @VisibleForTesting
    static boolean startScheduledProcessing(
            BackgroundSchedulerProcessor bridge,
            Context context,
            PersistableBundle taskExtras,
            Callback<Boolean> callback) {
        // Gather UMA data to measure how often the user's machine is amenable to background
        // loading when we wake to do a task.
        DeviceConditions deviceConditions = DeviceConditions.getCurrent(context);
        return bridge.startScheduledProcessing(deviceConditions, callback);
    }

    /**
     * @return Whether conditions for running the tasks are met.
     */
    @VisibleForTesting
    static boolean checkConditions(Context context, PersistableBundle taskExtras) {
        TriggerConditions triggerConditions =
                TaskExtrasPacker.unpackTriggerConditionsFromBundle(taskExtras);

        DeviceConditions deviceConditions = DeviceConditions.getCurrent(context);
        if (!areBatteryConditionsMet(deviceConditions, triggerConditions)) {
            Log.d(TAG, "Battery percentage is lower than minimum to start processing");
            return false;
        }

        if (!isSvelteConditionsMet()) {
            Log.d(TAG, "Application visible on low-end device so deferring background processing");
            return false;
        }

        return true;
    }

    /** Whether battery conditions (on power and enough battery percentage) are met. */
    private static boolean areBatteryConditionsMet(
            DeviceConditions deviceConditions, TriggerConditions triggerConditions) {
        return deviceConditions.isPowerConnected()
                || (deviceConditions.getBatteryPercentage()
                        >= triggerConditions.getMinimumBatteryPercentage());
    }

    /** Whether there are no visible activities when on Svelte. */
    private static boolean isSvelteConditionsMet() {
        return !SysUtils.isLowEndDevice() || !ApplicationStatus.hasVisibleActivities();
    }
}
