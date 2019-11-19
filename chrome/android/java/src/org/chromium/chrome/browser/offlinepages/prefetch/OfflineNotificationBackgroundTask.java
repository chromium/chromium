// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.content.Context;
import android.os.Build;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.DeviceConditions;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask.StartBeforeNativeResult;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.BackgroundTask.TaskFinishedCallback;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

import java.util.Calendar;

/**
 * Detects when the user has been offline and notifies them if they have offline content.
 *
 * The task has the following properties:
 * - If no fresh content exists, no work will be done.
 * - If fresh content exists, we will poll hourly until we detect
 *   an offline state.  Then,
 *     - Poll 4 times at a 15 minute delay, or until online again.
 *     - At the 4th time, we believe the user has been offline for
 *       an hour.
 *     - Notify the user about the fresh offline content.
 *
 *  We do a lot of work, such as storing counters in shared preferences, to avoid starting
 *  native and paying the cost of that startup, so this task is as efficient as possible.
 */
@JNINamespace("offline_pages::prefetch")
public class OfflineNotificationBackgroundTask extends NativeBackgroundTask {
    // When online, or when we first get fresh content, the polling delay is 60 minutes.
    public static final long DEFAULT_START_DELAY_MINUTES = 60;

    // When offline, the polling delay is 15 minutes, this will allow us to estimate "The user has
    // been offline for an hour".
    public static final long OFFLINE_POLL_DELAY_MINUTES = 15;

    // Number of notifications that, when ignored consecutively, will cause future notifications to
    // stop.
    static final int IGNORED_NOTIFICATION_MAX = 3;

    // Number of times we will poll offline before deciding that the user is "offline enough" to
    // show a notification.
    static final int OFFLINE_POLLING_ATTEMPTS = 4;

    // Detection mode used for rescheduling.
    // When online, we reschedule for |DEFAULT_START_DELAY_MINUTES|.
    static final int DETECTION_MODE_ONLINE = 0;

    // When offline, we reschedule for |OFFLINE_POLL_DELAY_MINUTES|.
    static final int DETECTION_MODE_OFFLINE = 1;

    // Testing instances, used for dependency injection.
    private static Calendar sCalendarForTesting;
    private static OfflinePageBridge sOfflinePageBridgeForTesting;

    // The native task finished callback.
    private TaskFinishedCallback mTaskFinishedCallback;

    public OfflineNotificationBackgroundTask() {}

    public static void scheduleTask(int detectionMode) {
        if (shouldNotReschedule()) {
            return;
        }
        long delayInMillis = delayForDetectionMode(detectionMode);
        long minimumDelay = minimumDelayInMillis();
        if (delayInMillis < minimumDelay) delayInMillis = minimumDelay;

        TaskInfo taskInfo =
                TaskInfo.createOneOffTask(TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID,
                                OfflineNotificationBackgroundTask.class,
                                // Minimum time to wait.
                                delayInMillis,
                                // Maximum time to wait.  After this interval the event will fire
                                // regardless of whether the conditions are right. Since there are
                                // no conditions we shouldn't get to this point.
                                delayInMillis * 2)
                        .setIsPersisted(true)
                        .setUpdateCurrent(true)
                        .build();
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfo);
    }

    public static void scheduleTaskWhenOnline() {
        PrefetchPrefs.setOfflineCounter(0);
        scheduleTask(DETECTION_MODE_ONLINE);
    }

    @CalledByNative
    private static void onFreshOfflineContentAvailable() {
        // When fresh offline content becomes available, the user is most likely online. We ignore
        // the rare case that the user might suddenly go offline.
        PrefetchPrefs.setHasNewPages(true);
        scheduleTaskWhenOnline();
    }

    @Override
    public void reschedule(Context context) {
        if (shouldNotReschedule()) {
            resetPrefs();
            return;
        }

        scheduleTask(DETECTION_MODE_ONLINE);
    }

    @Override
    public @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        if (shouldNotReschedule()) {
            resetPrefs();
            return StartBeforeNativeResult.DONE;
        }

        if (DeviceConditions.getCurrentNetConnectionType(context)
                != ConnectionType.CONNECTION_NONE) {
            scheduleTaskWhenOnline();

            // We schedule ourselves and return DONE because we want to reschedule using the normal
            // 1 hour timeout rather than Android's default 30s * 2^n exponential backoff schedule.
            return StartBeforeNativeResult.DONE;
        }

        int offlineCounter = PrefetchPrefs.getOfflineCounter();
        offlineCounter++;
        PrefetchPrefs.setOfflineCounter(offlineCounter);
        if (offlineCounter < OFFLINE_POLLING_ATTEMPTS) {
            scheduleTask(DETECTION_MODE_OFFLINE);
            return StartBeforeNativeResult.DONE;
        }

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        mTaskFinishedCallback = callback;
        OfflinePageBridge bridge = getOfflinePageBridge();
        PrefetchedPagesNotifier.getInstance().recordNotificationAction(
                PrefetchedPagesNotifier.NOTIFICATION_ACTION_MAY_SHOW);
        bridge.checkForNewOfflineContent(
                PrefetchPrefs.getNotificationLastShownTime(), (origin) -> doneContentCheck(origin));
    }

    private void doneContentCheck(String contentHost) {
        resetPrefs();
        mTaskFinishedCallback.taskFinished(false);

        if (!contentHost.isEmpty()) {
            PrefetchPrefs.setNotificationLastShownTime(getCurrentTimeMillis());
            PrefetchedPagesNotifier.getInstance().showNotification(contentHost);
        }

        // There is either no fresh content, or we just showed a notification - which implies there
        // is no more fresh content.  Clear the state in the background task scheduler.
        cancelTask();
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        return true;
    }

    private static long delayForDetectionMode(int detectionMode) {
        switch (detectionMode) {
            case DETECTION_MODE_ONLINE:
                return DateUtils.MINUTE_IN_MILLIS * DEFAULT_START_DELAY_MINUTES;
            case DETECTION_MODE_OFFLINE:
                return DateUtils.MINUTE_IN_MILLIS * OFFLINE_POLL_DELAY_MINUTES;
            default:
                return -1;
        }
    }

    // Computes the delay required since the last notification.  The spec says that notification
    // should not occur until 7am the day after the last notification, so we use the Calendar API to
    // figure out that time, in the local time zone.
    private static long minimumDelayInMillis() {
        long timeOfLastNotification = PrefetchPrefs.getNotificationLastShownTime();
        if (timeOfLastNotification <= 0) {
            return 0;
        }
        long currentTime = getCurrentTimeMillis();

        // The clock was set backwards - fix the PrefetchPref so that we don't wait until the
        // previous timeout.
        if (timeOfLastNotification > currentTime) {
            timeOfLastNotification = currentTime;
            PrefetchPrefs.setNotificationLastShownTime(currentTime);
        }

        Calendar c = getCalendar();
        c.setTimeInMillis(timeOfLastNotification);
        c.add(Calendar.DATE, 1);
        c.set(Calendar.HOUR_OF_DAY, 7);
        c.set(Calendar.MINUTE, 0);
        c.set(Calendar.SECOND, 0);
        c.set(Calendar.MILLISECOND, 0);
        long earliestNotificationTime = c.getTimeInMillis();
        if (earliestNotificationTime <= currentTime) {
            return 0;
        }

        return earliestNotificationTime - currentTime;
    }

    private static long getCurrentTimeMillis() {
        return getCalendar().getTimeInMillis();
    }

    private static void cancelTask() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(ContextUtils.getApplicationContext(),
                TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID);
    }

    private static boolean shouldNotReschedule() {
        boolean noNewPages = !PrefetchPrefs.getHasNewPages();
        boolean tooManyIgnoredNotifications =
                PrefetchPrefs.getIgnoredNotificationCounter() >= IGNORED_NOTIFICATION_MAX;

        // Always enable on O+ devices because notification settings are handled at the system
        // level, so the value of this pref can be ignored.
        boolean disabledByPref = Build.VERSION.SDK_INT < Build.VERSION_CODES.O
                && !PrefetchPrefs.getNotificationEnabled();

        return noNewPages || tooManyIgnoredNotifications || disabledByPref;
    }

    private void resetPrefs() {
        PrefetchPrefs.setOfflineCounter(0);
        PrefetchPrefs.setHasNewPages(false);
    }

    // This returns a new instance of Calendar, initialized to the same time as |sCalendar| for
    // testing purposes.
    private static Calendar getCalendar() {
        Calendar c = Calendar.getInstance();
        if (sCalendarForTesting != null) {
            c.setTimeInMillis(sCalendarForTesting.getTimeInMillis());
        }
        return c;
    }

    private static OfflinePageBridge getOfflinePageBridge() {
        if (sOfflinePageBridgeForTesting != null) {
            return sOfflinePageBridgeForTesting;
        }
        return OfflinePageBridge.getForProfile(Profile.getLastUsedProfile());
    }

    @VisibleForTesting
    static void setOfflinePageBridgeForTesting(OfflinePageBridge bridge) {
        sOfflinePageBridgeForTesting = bridge;
    }

    @VisibleForTesting
    static void setCalendarForTesting(Calendar c) {
        sCalendarForTesting = c;
    }
}
