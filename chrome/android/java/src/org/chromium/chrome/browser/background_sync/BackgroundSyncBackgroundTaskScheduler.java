// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import android.os.PersistableBundle;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TimeUtils;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * The {@link BackgroundSyncBackgroundTaskScheduler} singleton is responsible
 * for scheduling and cancelling background tasks to wake Chrome up so that
 * Background Sync events ready to be fired can be fired.
 *
 * Thread model: This class is to be run on the UI thread only.
 */
public class BackgroundSyncBackgroundTaskScheduler {
    /** An observer interface for BackgroundSyncBackgroundTaskScheduler. */
    interface Observer {
        void oneOffTaskScheduledFor(@BackgroundSyncTask int taskType, long delay);

        void oneOffTaskCanceledFor(@BackgroundSyncTask int taskType);
    }

    /**
     * Any tasks scheduled using GCMNetworkManager directly to wake up Chrome
     * would use this TASK_TAG. We no longer use GCMNetworkManager directly, so
     * when these tasks are run, we rescheduling using
     * BackgroundSyncBackgroundTaskScheduler.
     */
    public static final String TASK_TAG = "BackgroundSync Event";

    /**
     * Denotes the one-off Background Sync Background Tasks scheduled through
     * this class.
     * ONE_SHOT_SYNC_CHROME_WAKE_UP is the task that processes one-shot
     * Background Sync registrations.
     * PERIODIC_SYNC_CHROME_WAKE_UP processes Periodic Background Sync
     * registrations.
     */
    @IntDef({
        BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP,
        BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP
    })
    public @interface BackgroundSyncTask {
        int ONE_SHOT_SYNC_CHROME_WAKE_UP = 0;
        int PERIODIC_SYNC_CHROME_WAKE_UP = 1;
    };

    // Keep in sync with the default min_sync_recovery_time of
    // BackgroundSyncParameters.
    private static final long MIN_SYNC_RECOVERY_TIME = DateUtils.MINUTE_IN_MILLIS * 6;

    // Bundle key for the timestamp of the soonest wakeup time expected for
    // this task.
    public static final String SOONEST_EXPECTED_WAKETIME = "SoonestWakeupTime";

    private static BackgroundSyncBackgroundTaskScheduler sInstance;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    @CalledByNative
    public static BackgroundSyncBackgroundTaskScheduler getInstance() {
        if (sInstance == null) sInstance = new BackgroundSyncBackgroundTaskScheduler();
        return sInstance;
    }

    @VisibleForTesting
    static boolean hasInstance() {
        return sInstance != null;
    }

    /**
     * Returns the appropriate TaskID to use based on the class of the Background
     * Sync task we're working with.
     *
     * @param taskType The Background Sync task to get the TaskID for.
     */
    @VisibleForTesting
    public static int getAppropriateTaskId(@BackgroundSyncTask int taskType) {
        switch (taskType) {
            case BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP:
                return TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID;
            case BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP:
                return TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID;
            default:
                assert false : "Incorrect Background Sync task type";
                return -1;
        }
    }

    /** @param observer The observer to add. */
    @VisibleForTesting
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Cancels a Background Sync one-off task, if there's one scheduled.
     *
     * @param taskType The Background Sync task to cancel.
     */
    @VisibleForTesting
    @CalledByNative
    protected void cancelOneOffTask(@BackgroundSyncTask int taskType) {
        BackgroundTaskSchedulerFactory.getScheduler()
                .cancel(ContextUtils.getApplicationContext(), getAppropriateTaskId(taskType));

        for (Observer observer : mObservers) {
            observer.oneOffTaskCanceledFor(taskType);
        }
    }

    /**
     * Schedules a one-off background task to wake the browser up on network
     * connectivity and call into native code to fire ready (periodic) Background Sync
     * events.
     *
     * @param minDelayMs The minimum time to wait before waking the browser.
     * @param taskType   The Background Sync task to schedule.
     */
    @VisibleForTesting
    @CalledByNative
    protected boolean scheduleOneOffTask(@BackgroundSyncTask int taskType, long minDelayMs) {
        // Pack SOONEST_EXPECTED_WAKETIME in extras.
        PersistableBundle taskExtras = new PersistableBundle();
        taskExtras.putLong(SOONEST_EXPECTED_WAKETIME, System.currentTimeMillis() + minDelayMs);

        // We setWindowEndTime to Long.MAX_VALUE to wait a long time for network connectivity,
        // so that we can process the pending sync event. setExpiresAfterWindowEndTime ensures
        // that we never wake up Chrome without network connectivity.
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(minDelayMs)
                        .setWindowEndTimeMs(TimeUtils.MILLISECONDS_PER_YEAR)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo taskInfo =
                TaskInfo.createTask(getAppropriateTaskId(taskType), timingInfo)
                        .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                        .setUpdateCurrent(true)
                        .setIsPersisted(true)
                        .setExtras(taskExtras)
                        .build();
        // This will overwrite any existing task with this ID.
        boolean didSchedule =
                BackgroundTaskSchedulerFactory.getScheduler()
                        .schedule(ContextUtils.getApplicationContext(), taskInfo);

        for (Observer observer : mObservers) {
            observer.oneOffTaskScheduledFor(taskType, minDelayMs);
        }

        return didSchedule;
    }

    /**
     * Method for rescheduling a background task to wake up Chrome for processing Background Sync
     * events in the event of an OS upgrade or Google Play Services upgrade.
     *
     * @param taskType The Background Sync task to reschedule.
     */
    public void reschedule(@BackgroundSyncTask int taskType) {
        // TODO(crbug.com/40256221): Investigate if this can be deleted.
        scheduleOneOffTask(taskType, MIN_SYNC_RECOVERY_TIME);
    }

    @NativeMethods
    interface Natives {
        /**
         * Chrome currently disables BackgroundSyncManager if Google Play Services aren't up-to-date
         * at startup. Disable this check for tests, since we mock out interaction with GCM.
         * This method can be removed once our test devices start updating Google Play Services
         * before tests are run. https://crbug.com/514449
         * @param disabled disable or enable the version check for Google Play Services.
         */
        void setPlayServicesVersionCheckDisabledForTests(boolean disabled);
    }
}
