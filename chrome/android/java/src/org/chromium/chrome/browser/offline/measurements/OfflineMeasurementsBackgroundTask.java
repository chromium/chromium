// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline.measurements;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.concurrent.TimeUnit;

/**
 * Collects data about how often the user is offline, and what they do while offline.
 */
public class OfflineMeasurementsBackgroundTask implements BackgroundTask {
    // Finch parameters and default values.
    public static final String MEASUREMENT_INTERVAL_IN_MINUTES = "measurement_interval_in_minutes";
    public static final int DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES = 60;

    private static int sNewMeasurementIntervalInMinutesTestingOverride;

    // UMA histograms.
    public static final String OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL =
            "Offline.Measurements.MeasurementInterval";
    public static final String OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS =
            "Offline.Measurements.TimeBetweenChecks";

    /**
     * Clock to use so we can mock the time in tests.
     */
    public interface Clock {
        long currentTimeMillis();
    }
    private static Clock sClock = System::currentTimeMillis;

    @VisibleForTesting
    static void setClockForTesting(Clock clock) {
        sClock = clock;
    }

    public OfflineMeasurementsBackgroundTask() {}

    public static void maybeScheduleTaskAndReportMetrics() {
        reportMetrics();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK)) {
            scheduleTask();
        } else {
            cancelTaskAndClearPersistedMetrics();
        }
    }

    private static void reportMetrics() {
        // Log all stored values in Prefs, then clear prefs.
        long[] timeBetweenChecksMillisList = getTimeBetweenChecksFromPrefs();
        for (long timeBetweenChecksMillis : timeBetweenChecksMillisList) {
            if (timeBetweenChecksMillis >= 0) {
                RecordHistogram.recordCustomTimesHistogram(OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS,
                        timeBetweenChecksMillis, TimeUnit.MINUTES.toMillis(1),
                        TimeUnit.DAYS.toMillis(1), 50);
            }
        }

        // After logging the data to UMA, clear the data from prefs so it isn't logged again.
        clearTimeBetweenChecksFromPrefs();
    }

    private static void scheduleTask() {
        int newMeasurementIntervalInMinutes = getNewMeasurementIntervalInMinutes();
        int currentTaskMeasurementIntervalInMinutes = getCurrentTaskMeasurementIntervalInMinutes();

        // If the current task has the same parameters as the parameter from Finch, then we don't
        // have to do anything.
        if (currentTaskMeasurementIntervalInMinutes == newMeasurementIntervalInMinutes) {
            return;
        }

        // If the parameter stored in Prefs is non-zero, then the task is already running. In that
        // case we need to cancel it first so we can schedule it with the new parameter.
        if (currentTaskMeasurementIntervalInMinutes != 0) {
            cancelTaskAndClearPersistedMetrics();
        }

        // Schedule the task with the new interval.
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TimeUnit.MINUTES.toMillis(newMeasurementIntervalInMinutes))
                        .build();

        TaskInfo taskInfo =
                TaskInfo.createTask(TaskIds.OFFLINE_MEASUREMENT_JOB_ID, timingInfo).build();

        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfo);

        RecordHistogram.recordCustomTimesHistogram(OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL,
                TimeUnit.MINUTES.toMillis(newMeasurementIntervalInMinutes),
                TimeUnit.MINUTES.toMillis(1), TimeUnit.DAYS.toMillis(1), 50);

        setCurrentTaskMeasurementIntervalInMinutes(newMeasurementIntervalInMinutes);
        setLastCheckMillis(-1);
    }

    private static void cancelTaskAndClearPersistedMetrics() {
        // Cancels the task if is currently running.
        long currentTaskMeasurementIntervalInMinutes = getCurrentTaskMeasurementIntervalInMinutes();
        if (currentTaskMeasurementIntervalInMinutes != 0) {
            BackgroundTaskSchedulerFactory.getScheduler().cancel(
                    ContextUtils.getApplicationContext(), TaskIds.OFFLINE_MEASUREMENT_JOB_ID);
        }

        // Clears the state stored in Prefs.
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS);
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys
                        .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES);
    }

    private static int getNewMeasurementIntervalInMinutes() {
        if (sNewMeasurementIntervalInMinutesTestingOverride > 0) {
            return sNewMeasurementIntervalInMinutesTestingOverride;
        }

        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK,
                MEASUREMENT_INTERVAL_IN_MINUTES, DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES);
    }

    @VisibleForTesting
    static void setNewMeasurementIntervalInMinutesForTesting(int measurementIntervalInMinutes) {
        sNewMeasurementIntervalInMinutesTestingOverride = measurementIntervalInMinutes;
    }

    /** Writes the interval used to schedule the current task to Prefs. */
    private static void setCurrentTaskMeasurementIntervalInMinutes(
            int currentTaskMeasurementIntervalInMinutes) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys
                        .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES,
                currentTaskMeasurementIntervalInMinutes);
    }

    /**
     * Gets the value of the current measurement interval stored in Prefs. If the task is not
     * running, this will be zero. If the task is running already, it will be the Finch parameter
     * used to start it.
     */
    private static int getCurrentTaskMeasurementIntervalInMinutes() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys
                        .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES);
    }

    /** Writes the time (in milliseconds) of the last background check to Prefs. */
    private static void setLastCheckMillis(long lastCheckMillis) {
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS, lastCheckMillis);
    }

    /** Gets the time (in milliseconds) of the last background check to Prefs. */
    private static long getLastCheckMillis() {
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS);
    }

    @Override
    public void reschedule(Context context) {
        scheduleTask();
    }

    @Override
    public boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        // If feature is no longer enabled, cancels the periodic task so it doesn't run again.
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK)) {
            cancelTaskAndClearPersistedMetrics();
            return false;
        }

        // Calculates the time since the last time this task was run and record it to UMA.
        long lastCheckMillis = getLastCheckMillis();
        long currentCheckMillis = sClock.currentTimeMillis();
        setLastCheckMillis(currentCheckMillis);

        if (lastCheckMillis > 0) {
            long timeBetweenChecksMillis = currentCheckMillis - lastCheckMillis;
            addTimeBetweenChecksToPrefs(timeBetweenChecksMillis);
        }
        return false;
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // Informs the scheduler that the task does not need to be rescheduled.
        return false;
    }

    private static String getTimeBetweenChecksFromPrefsAsString() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST, "");
    }

    private static void addTimeBetweenChecksToPrefs(long newValue) {
        String existingList = getTimeBetweenChecksFromPrefsAsString();

        // Add newValue to the list.
        StringBuilder strBuilder = new StringBuilder(existingList);
        if (strBuilder.length() > 0) {
            strBuilder.append(",");
        }
        strBuilder.append(newValue);

        // Write the new list to Prefs.
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST,
                strBuilder.toString());
    }

    private static long[] getTimeBetweenChecksFromPrefs() {
        String rawList = getTimeBetweenChecksFromPrefsAsString();

        if (rawList.equals("")) {
            return new long[0];
        }

        // Split list by "," and then convert to long. Any values that cannot be converted will
        // return a value of -1.
        String[] longAsStrings = rawList.split(",");
        long[] longAsLongs = new long[longAsStrings.length];
        for (int i = 0; i < longAsStrings.length; i++) {
            try {
                longAsLongs[i] = Long.parseLong(longAsStrings[i]);
            } catch (NumberFormatException e) {
                longAsLongs[i] = -1;
            }
        }
        return longAsLongs;
    }

    private static void clearTimeBetweenChecksFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST);
    }
}
