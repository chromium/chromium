// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTask;
import org.chromium.chrome.browser.background_sync.PeriodicBackgroundSyncChromeWakeUpTask;
import org.chromium.chrome.browser.component_updater.UpdateTask;
import org.chromium.chrome.browser.download.DownloadResumptionBackgroundTask;
import org.chromium.chrome.browser.download.service.DownloadBackgroundTask;
import org.chromium.chrome.browser.explore_sites.ExploreSitesBackgroundTask;
import org.chromium.chrome.browser.feed.FeedRefreshTask;
import org.chromium.chrome.browser.notifications.NotificationTriggerBackgroundTask;
import org.chromium.chrome.browser.notifications.scheduler.NotificationSchedulerTask;
import org.chromium.chrome.browser.offlinepages.OfflineBackgroundTask;
import org.chromium.chrome.browser.offlinepages.prefetch.OfflineNotificationBackgroundTask;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchBackgroundTask;
import org.chromium.chrome.browser.omaha.OmahaService;
import org.chromium.chrome.browser.services.gcm.GCMBackgroundTask;
import org.chromium.chrome.browser.webapps.WebApkUpdateTask;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;

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
        switch (taskId) {
            case TaskIds.OMAHA_JOB_ID:
                return new OmahaService();
            case TaskIds.GCM_BACKGROUND_TASK_JOB_ID:
                return new GCMBackgroundTask();
            case TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID:
                return new OfflineBackgroundTask();
            case TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID:
                return new PrefetchBackgroundTask();
            case TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID:
                return new OfflineNotificationBackgroundTask();
            case TaskIds.DOWNLOAD_SERVICE_JOB_ID:
            case TaskIds.DOWNLOAD_CLEANUP_JOB_ID:
            case TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID:
                return new DownloadBackgroundTask();
            case TaskIds.WEBAPK_UPDATE_JOB_ID:
                return new WebApkUpdateTask();
            case TaskIds.DOWNLOAD_RESUMPTION_JOB_ID:
                return new DownloadResumptionBackgroundTask();
            case TaskIds.FEED_REFRESH_JOB_ID:
                return new FeedRefreshTask();
            case TaskIds.COMPONENT_UPDATE_JOB_ID:
                return new UpdateTask();
            case TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID:
            case TaskIds.EXPLORE_SITES_REFRESH_JOB_ID:
                return new ExploreSitesBackgroundTask();
            case TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID:
                return new BackgroundSyncBackgroundTask();
            case TaskIds.NOTIFICATION_SCHEDULER_JOB_ID:
                return new NotificationSchedulerTask();
            case TaskIds.NOTIFICATION_TRIGGER_JOB_ID:
                return new NotificationTriggerBackgroundTask();
            case TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID:
                return new PeriodicBackgroundSyncChromeWakeUpTask();
            // When adding a new job id with a BackgroundTask, remember to add a specific case for
            // it here.
            default:
                Log.w(TAG, "Unable to find BackgroundTask class for task id " + taskId);
                return null;
        }
    }
}
