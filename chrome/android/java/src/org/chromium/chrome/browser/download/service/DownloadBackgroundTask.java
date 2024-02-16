// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadNotificationService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileKeyUtil;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.DownloadTaskType;
import org.chromium.components.download.internal.BatteryStatusListenerAndroid;

/**
 * Entry point for the download service to perform desired action when the task is fired by the
 * scheduler.
 */
@JNINamespace("download::android")
public class DownloadBackgroundTask extends NativeBackgroundTask {
    @DownloadTaskType private int mCurrentTaskType;

    @Override
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        boolean requiresCharging =
                taskParameters
                        .getExtras()
                        .getBoolean(DownloadTaskScheduler.EXTRA_BATTERY_REQUIRES_CHARGING);
        int optimalBatteryPercentage =
                taskParameters
                        .getExtras()
                        .getInt(DownloadTaskScheduler.EXTRA_OPTIMAL_BATTERY_PERCENTAGE);
        mCurrentTaskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        // Reschedule if minimum battery level is not satisfied.
        if (!requiresCharging
                && BatteryStatusListenerAndroid.getBatteryPercentage() < optimalBatteryPercentage) {
            return StartBeforeNativeResult.RESCHEDULE;
        }

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, final TaskFinishedCallback callback) {
        // In case of future upgrades, we would need to build an intent for the old version and
        // validate that this code still works. This would require decoupling this immediate class
        // from native as well.
        DownloadManagerService.getDownloadManagerService().initForBackgroundTask();
        if (DownloadUtils.isUserInitiatedJob(mTaskId)) {
            // In case of user-initiated jobs, we need to ensure that notifications are attached to
            // the job life cycle.
            ensureNotificationBridgeInitialized();
            DownloadNotificationService.getInstance()
                    .setBackgroundTaskNotificationCallback(taskParameters.getTaskId(), callback);
        }
        DownloadBackgroundTaskJni.get()
                .startBackgroundTask(
                        DownloadBackgroundTask.this,
                        getProfileKey(),
                        mCurrentTaskType,
                        needsReschedule -> {
                            finishTask(taskParameters, callback, needsReschedule);
                        });
    }

    @Override
    protected boolean supportsMinimalBrowser() {
        return true;
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        if (DownloadUtils.isUserInitiatedJob(mTaskId)) {
            DownloadNotificationService.getInstance()
                    .setBackgroundTaskNotificationCallback(taskParameters.getTaskId(), null);
        }
        @DownloadTaskType
        int taskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        return DownloadBackgroundTaskJni.get()
                .stopBackgroundTask(DownloadBackgroundTask.this, getProfileKey(), taskType);
    }

    @VisibleForTesting
    protected void finishTask(
            TaskParameters taskParameters, TaskFinishedCallback callback, boolean needsReschedule) {
        if (DownloadUtils.isUserInitiatedJob(mTaskId)) {
            DownloadNotificationService.getInstance()
                    .setBackgroundTaskNotificationCallback(taskParameters.getTaskId(), null);
        }
        callback.taskFinished(needsReschedule);
    }

    @VisibleForTesting()
    protected ProfileKey getProfileKey() {
        return ProfileKeyUtil.getLastUsedRegularProfileKey();
    }

    @VisibleForTesting
    protected void ensureNotificationBridgeInitialized() {
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        void startBackgroundTask(
                DownloadBackgroundTask caller,
                ProfileKey key,
                int taskType,
                Callback<Boolean> callback);

        boolean stopBackgroundTask(DownloadBackgroundTask caller, ProfileKey key, int taskType);
    }
}
