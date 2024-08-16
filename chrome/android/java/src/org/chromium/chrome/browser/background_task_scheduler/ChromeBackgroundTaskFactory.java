// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import org.jni_zero.CalledByNative;

import org.chromium.base.Log;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTask;
import org.chromium.chrome.browser.background_sync.PeriodicBackgroundSyncChromeWakeUpTask;
import org.chromium.chrome.browser.download.service.DownloadBackgroundTask;
import org.chromium.chrome.browser.notifications.NotificationTriggerBackgroundTask;
import org.chromium.chrome.browser.notifications.scheduler.NotificationSchedulerTask;
import org.chromium.chrome.browser.offlinepages.OfflineBackgroundTask;
import org.chromium.chrome.browser.omaha.OmahaService;
import org.chromium.chrome.browser.safety_hub.SafetyHubFetchTask;
import org.chromium.chrome.browser.services.gcm.GCMNativeBackgroundTask;
import org.chromium.chrome.browser.webapps.WebApkUpdateTask;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.component_updater.UpdateTask;

/**
 * Implementation of {@link BackgroundTaskFactory} for //chrome.
 * Maps all task ids used in //chrome with their BackgroundTask classes.
 */
public class ChromeBackgroundTaskFactory implements BackgroundTaskFactory {
    private static final String TAG = "ChromeBkgrdTaskF";

    private ChromeBackgroundTaskFactory() {}

    private static class LazyHolder {
        static final ChromeBackgroundTaskFactory INSTANCE = new ChromeBackgroundTaskFactory();
    }

    @CalledByNative
    public static void setAsDefault() {
        BackgroundTaskSchedulerFactory.setBackgroundTaskFactory(LazyHolder.INSTANCE);
    }

    @Override
    public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
        BackgroundTask backgroundTask = createBackgroundTaskFromTaskId(taskId);
        if (backgroundTask instanceof NativeBackgroundTask) {
            ((NativeBackgroundTask) backgroundTask)
                    .setDelegate(new ChromeNativeBackgroundTaskDelegate());
        }

        return backgroundTask;
    }

    private BackgroundTask createBackgroundTaskFromTaskId(int taskId) {
        switch (taskId) {
            case TaskIds.OMAHA_JOB_ID:
                return new OmahaService();
            case TaskIds.GCM_BACKGROUND_TASK_JOB_ID:
                return new GCMNativeBackgroundTask();
            case TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID:
                return new OfflineBackgroundTask();
            case TaskIds.DOWNLOAD_SERVICE_JOB_ID:
            case TaskIds.DOWNLOAD_CLEANUP_JOB_ID:
            case TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID:
            case TaskIds.DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID:
            case TaskIds.DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_JOB_ID:
            case TaskIds.DOWNLOAD_LATER_JOB_ID:
                return new DownloadBackgroundTask();
            case TaskIds.WEBAPK_UPDATE_JOB_ID:
                return new WebApkUpdateTask();
            case TaskIds.COMPONENT_UPDATE_JOB_ID:
                return new UpdateTask();
            case TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID:
                return new BackgroundSyncBackgroundTask();
            case TaskIds.NOTIFICATION_SCHEDULER_JOB_ID:
                return new NotificationSchedulerTask();
            case TaskIds.NOTIFICATION_TRIGGER_JOB_ID:
                return new NotificationTriggerBackgroundTask();
            case TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID:
                return new PeriodicBackgroundSyncChromeWakeUpTask();
            case TaskIds.SAFETY_HUB_JOB_ID:
                return new SafetyHubFetchTask();
                // End of Java tasks. All native tasks should be listed here.
            case TaskIds.QUERY_TILE_JOB_ID:
            case TaskIds.FEEDV2_REFRESH_JOB_ID:
            case TaskIds.WEBFEEDS_REFRESH_JOB_ID:
                return new ProxyNativeTask();
                // When adding a new job id with a BackgroundTask, remember to add a specific case
                // for it here.
                // If the job id corresponds to a native task, use {@link ProxyNativeTask} as the
                // task here and also update
                // ChromeBackgroundTaskFactory::GetNativeBackgroundTaskFromTaskId
                // to link to the real task.
            default:
                Log.w(TAG, "Unable to find BackgroundTask class for task id " + taskId);
                return null;
        }
    }
}
