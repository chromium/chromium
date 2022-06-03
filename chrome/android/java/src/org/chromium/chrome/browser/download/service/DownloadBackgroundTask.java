// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.DownloadTaskType;
import org.chromium.components.download.internal.BatteryStatusListenerAndroid;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Entry point for the download service to perform desired action when the task is fired by the
 * scheduler.
 */
@JNINamespace("download::android")
public class DownloadBackgroundTask extends NativeBackgroundTask {
    @DownloadTaskType
    private int mCurrentTaskType;

    // Whether only the minimal browser is required to start.
    private boolean mStartsMinimalBrowser;

    @Override
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        boolean requiresCharging = taskParameters.getExtras().getBoolean(
                DownloadTaskScheduler.EXTRA_BATTERY_REQUIRES_CHARGING);
        int optimalBatteryPercentage = taskParameters.getExtras().getInt(
                DownloadTaskScheduler.EXTRA_OPTIMAL_BATTERY_PERCENTAGE);
        mCurrentTaskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        // The feature value could change during native initialization, store it first.
        mStartsMinimalBrowser = (mCurrentTaskType == DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK
                                        || mCurrentTaskType == DownloadTaskType.DOWNLOAD_LATER_TASK)
                ? CachedFeatureFlags.isEnabled(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD)
                : PrefetchConfiguration.isMinimalBrowserForBackgroundPrefetchEnabled();
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
        assert BrowserStartupController.getInstance().isFullBrowserStarted()
                || mStartsMinimalBrowser;
        DownloadManagerService.getDownloadManagerService().initForBackgroundTask();
        ProfileKey key = ProfileKey.getLastUsedRegularProfileKey();
        DownloadBackgroundTaskJni.get().startBackgroundTask(DownloadBackgroundTask.this, key,
                mCurrentTaskType, needsReschedule -> callback.taskFinished(needsReschedule));
    }

    @Override
    protected boolean supportsMinimalBrowser() {
        return mStartsMinimalBrowser;
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        @DownloadTaskType
        int taskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        ProfileKey key = ProfileKey.getLastUsedRegularProfileKey();
        return DownloadBackgroundTaskJni.get().stopBackgroundTask(
                DownloadBackgroundTask.this, key, taskType);
    }

    @Override
    public void reschedule(Context context) {
        DownloadTaskScheduler.rescheduleAllTasks();
    }

    @NativeMethods
    interface Natives {
        void startBackgroundTask(DownloadBackgroundTask caller, ProfileKey key, int taskType,
                Callback<Boolean> callback);
        boolean stopBackgroundTask(DownloadBackgroundTask caller, ProfileKey key, int taskType);
    }
}
