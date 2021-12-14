// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.measurements;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.PowerManager;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Base64;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.offline_pages.measurements.proto.OfflineMeasurementsProto.SystemState;
import org.chromium.chrome.browser.offline_pages.measurements.proto.OfflineMeasurementsProto.SystemStateList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Calendar;
import java.util.concurrent.TimeUnit;

/**
 * Collects data about how often the user is offline, and what they do while offline.
 */
public class OfflineMeasurementsBackgroundTask implements BackgroundTask {
    // Finch parameters and default values.
    public static final String MEASUREMENT_INTERVAL_IN_MINUTES = "measurement_interval_in_minutes";
    public static final int DEFAULT_MEASUREMENT_INTERVAL_IN_MINUTES = 60;

    private static final String HTTP_PROBE_URL = "http_probe_url";
    private static final String DEFAULT_HTTP_PROBE_URL = "https://www.google.com/generate_204";

    private static final String HTTP_PROBE_TIMEOUT_MS = "http_probe_timeout_ms";
    private static final int DEFAULT_HTTP_PROBE_TIMEOUT_MS = 5000;

    private static final String HTTP_PROBE_METHOD = "http_probe_method";
    private static final String DEFAULT_HTTP_PROBE_METHOD = "GET";

    // HTTP probe constant.
    private static final String USER_AGENT_HEADER_NAME = "User-Agent";

    // Testing overrides
    private static int sNewMeasurementIntervalInMinutesTestingOverride;
    private static Boolean sIsAirplaneModeEnabledTestingOverride;
    private static Boolean sIsRoamingTestingOverride;
    private static Boolean sIsInteractiveTestingOverride;
    private static Boolean sIsApplicationForegroundTestingOverride;

    // UMA histograms.
    public static final String OFFLINE_MEASUREMENTS_MEASUREMENT_INTERVAL =
            "Offline.Measurements.MeasurementInterval";
    public static final String OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS =
            "Offline.Measurements.TimeBetweenChecks";
    public static final String OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT =
            "Offline.Measurements.HttpProbeResult";
    public static final String OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED =
            "Offline.Measurements.IsAirplaneModeEnabled";
    public static final String OFFLINE_MEASUREMENTS_IS_ROAMING = "Offline.Measurements.IsRoaming";
    public static final String OFFLINE_MEASUREMENTS_USER_STATE = "Offline.Measurements.UserState";

    // The result of the HTTP probing. Defined in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. These values are also defined in
    // chrome/browser/offline_pages/measurements/proto/system_state.proto.
    @IntDef({ProbeResult.INVALID, ProbeResult.NO_INTERNET, ProbeResult.SERVER_ERROR,
            ProbeResult.UNEXPECTED_RESPONSE, ProbeResult.VALIDATED, ProbeResult.CANCELLED,
            ProbeResult.MULTIPLE_URL_CONNECTIONS_OPEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProbeResult {
        // Value could not be parsed from Prefs.
        int INVALID = 0;
        // The HTTP probe could not connect to the Internet.
        int NO_INTERNET = 1;
        // Server returns response code >= 400.
        int SERVER_ERROR = 2;
        // Received an unepxected response from the server. This is most likely from either a
        // captive protal or a broken transparent proxy.
        int UNEXPECTED_RESPONSE = 3;
        // Validated when the expected result is received from server.
        int VALIDATED = 4;
        // The HTTP probe was cancelled before it could finish, because the background task was
        // stopped.
        int CANCELLED = 5;
        // Multiple HttpURLConnections were running at the same time causing the HTTP probe to fail.
        int MULTIPLE_URL_CONNECTIONS_OPEN = 6;
        // Count.
        int RESULT_COUNT = 7;
    }

    // The state of the phone and how / if the user is interacting with it. Defined in
    // tools/metrics/histograms/enums.xml. These values are persisted to logs. Entries should not be
    // renumbered and numeric values should never be reused. These values are also defined in
    // chrome/browser/offline_pages/measurements/proto/system_state.proto.
    @IntDef({UserState.INVALID, UserState.PHONE_OFF, UserState.NOT_USING_PHONE,
            UserState.USING_CHROME})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UserState {
        // Value could not be parsed from Prefs.
        int INVALID = 0;
        // The user's phone was off..
        int PHONE_OFF = 1;
        // The user's phone screen is not interactive.
        int NOT_USING_PHONE = 2;
        // The user's phone screen is interactive and Chrome is not in the foreground.
        int USING_PHONE_NOT_CHROME = 3;
        // The user's phone screen is interactive and Chrome is in the foreground.
        int USING_CHROME = 4;
        // Count.
        int RESULT_COUNT = 5;
    }

    /**
     * Clock to use so we can mock the time in tests.
     */
    public static class Clock {
        long currentTimeMillis() {
            return System.currentTimeMillis();
        }

        long elapsedRealtime() {
            return SystemClock.elapsedRealtime();
        }

        int getLocalHourOfDay() {
            return Calendar.getInstance().get(Calendar.HOUR_OF_DAY);
        }
    }
    private static Clock sClock = new Clock();

    @VisibleForTesting
    static void setClockForTesting(Clock clock) {
        sClock = clock;
    }

    // Runs the HTTP probe.
    private AsyncTask<Integer> mHttpProbeAsyncTask;

    public OfflineMeasurementsBackgroundTask() {}

    public static void maybeScheduleTask() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK)) {
            scheduleTask();
        } else {
            cancelTaskAndClearPersistedMetrics();
        }
    }

    public static byte[] getPersistedSystemStateListAsBytes() {
        return getSystemStateListFromPrefs().toByteArray();
    }

    public static void reportMetricsToUmaAndClear() {
        SystemStateList systemStateList = getSystemStateListFromPrefs();

        // Record the data in the system state list to UMA.
        // TODO(1131600): Move the logging of UMA metrics to Native alongside the logging of metrics
        // to UKM.
        for (SystemState systemState : systemStateList.getSystemStatesList()) {
            if (systemState.hasTimeSinceLastCheckMillis()) {
                RecordHistogram.recordCustomTimesHistogram(OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS,
                        systemState.getTimeSinceLastCheckMillis(), TimeUnit.MINUTES.toMillis(1),
                        TimeUnit.DAYS.toMillis(1), 50);
            }

            if (systemState.hasUserState()) {
                RecordHistogram.recordEnumeratedHistogram(OFFLINE_MEASUREMENTS_USER_STATE,
                        systemState.getUserState().getNumber(), UserState.RESULT_COUNT);
            }

            if (systemState.hasProbeResult()) {
                RecordHistogram.recordEnumeratedHistogram(OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                        systemState.getProbeResult().getNumber(), ProbeResult.RESULT_COUNT);
            }

            if (systemState.hasIsRoaming()) {
                RecordHistogram.recordBooleanHistogram(
                        OFFLINE_MEASUREMENTS_IS_ROAMING, systemState.getIsRoaming());
            }

            if (systemState.hasIsAirplaneModeEnabled()) {
                RecordHistogram.recordBooleanHistogram(
                        OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED,
                        systemState.getIsAirplaneModeEnabled());
            }
        }

        // Clear the data from prefs so it isn't logged again.
        clearSystemStateListFromPrefs();
    }

    private static void scheduleTask() {
        updateHttpProbeParameters();

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

        TaskInfo taskInfo = TaskInfo.createTask(TaskIds.OFFLINE_MEASUREMENT_JOB_ID, timingInfo)
                                    .setIsPersisted(true)
                                    .build();

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
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_LAST_CHECK_MILLIS);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys
                        .OFFLINE_MEASUREMENTS_CURRENT_TASK_MEASUREMENT_INTERVAL_IN_MINUTES);
        clearHttpProbeParametersFromPrefs();
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

    /**
     * Gets the latest Finch parameters for the HTTP probe, then writes them to prefs if using a
     * non-default value.
     */
    private static void updateHttpProbeParameters() {
        // Clears any params currently stored in Prefs.
        clearHttpProbeParametersFromPrefs();

        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();

        // Gets the User Agent from ContentUtils.
        String userAgentString = ContentUtils.getBrowserUserAgent();
        sharedPreferencesManager.writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING, userAgentString);

        // Gets the parameters from Finch. If there is a value for a given parameter and it doesn't
        // match the default value, then it is written to Prefs.
        String httpProbeUrl = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, HTTP_PROBE_URL);
        if (!httpProbeUrl.isEmpty() && !httpProbeUrl.equals(DEFAULT_HTTP_PROBE_URL)) {
            sharedPreferencesManager.writeString(
                    ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL, httpProbeUrl);
        }

        int httpProbeTimeoutMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, HTTP_PROBE_TIMEOUT_MS,
                DEFAULT_HTTP_PROBE_TIMEOUT_MS);
        if (httpProbeTimeoutMs != DEFAULT_HTTP_PROBE_TIMEOUT_MS) {
            sharedPreferencesManager.writeInt(
                    ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS,
                    httpProbeTimeoutMs);
        }

        String httpProbeMethod = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.OFFLINE_MEASUREMENTS_BACKGROUND_TASK, HTTP_PROBE_METHOD);
        if (!httpProbeMethod.isEmpty() && !httpProbeMethod.equals(DEFAULT_HTTP_PROBE_METHOD)) {
            sharedPreferencesManager.writeString(
                    ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD, httpProbeMethod);
        }
    }

    /** Clears the HTTP probe parameters from Prefs. */
    private static void clearHttpProbeParametersFromPrefs() {
        // Clears the state stored in Prefs.
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS);
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD);
    }

    @VisibleForTesting
    static void setHttpProbeUrlForTesting(String httpProbeUrl) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL, httpProbeUrl);
    }

    @VisibleForTesting
    static void setHttpProbeMethodForTesting(String httpProbeMethod) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD, httpProbeMethod);
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

        boolean didSystemBootSinceLastCheck = false;
        SystemState.Builder partialSystemState = SystemState.newBuilder();
        if (lastCheckMillis > 0) {
            long timeSinceLastChecksMillis = currentCheckMillis - lastCheckMillis;

            long timeSinceBootMillis = sClock.elapsedRealtime();
            didSystemBootSinceLastCheck = timeSinceBootMillis < timeSinceLastChecksMillis;

            partialSystemState.setTimeSinceLastCheckMillis(timeSinceLastChecksMillis);
        }

        int localHourOfDay = sClock.getLocalHourOfDay();

        // Gets whether airplane mode is enabled or disabled.
        boolean isAirplaneModeEnabled = isAirplaneModeEnabled(context);
        boolean isInteractive = isInteractive(context);
        boolean isApplicationForeground = isApplicationForeground();

        int userState = convertToUserState(
                didSystemBootSinceLastCheck, isInteractive, isApplicationForeground);

        partialSystemState.setUserState(SystemState.UserState.forNumber(userState))
                .setIsAirplaneModeEnabled(isAirplaneModeEnabled)
                .setLocalHourOfDayStart(localHourOfDay);

        try {
            boolean isRoaming = isRoaming(context);
            partialSystemState.setIsRoaming(isRoaming);
        } catch (SecurityException e) {
            // When getting the capabilities of a network, we can encounter a SecurityException in
            // some cases. When this happens we cannot determine if the network is marked as roaming
            // or not roaming, so we do not record a value for IsRoaming. See crbug/1246848.
        }

        // Starts the HTTP probe.
        sendHttpProbe((Integer probeResult) -> {
            processResult(partialSystemState, probeResult, callback);
        });

        return true;
    }

    /**
     * Saves the result of the HTTP probe to Prefs, and informs the task scheduler that the task is
     * finished.
     * @param partialSystemState The portion of the system state that has already been determined.
     * @param probeResult The result of the HTTP probe as a |ProbeResult|.
     * @param callback The callback used to inform the background task scheduler that the task has
     * finished.
     */
    private void processResult(SystemState.Builder partialSystemState, Integer probeResult,
            TaskFinishedCallback callback) {
        // Adds the result of the HTTP probe to the system state proto, then write the full proto to
        // Prefs.
        addSystemStateToListInPrefs(
                partialSystemState.setProbeResult(SystemState.ProbeResult.forNumber(probeResult))
                        .build());

        // Informs scheduler that the background task has finished.
        callback.taskFinished(false);
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // Cancels the HTTP probe if it is still running.
        if (mHttpProbeAsyncTask != null) {
            mHttpProbeAsyncTask.cancel(true);
            addSystemStateToListInPrefs(SystemState.newBuilder()
                                                .setProbeResult(SystemState.ProbeResult.CANCELLED)
                                                .build());
        }

        // Informs the scheduler that the task does not need to be rescheduled.
        return false;
    }

    /**
     * Starts an HTTP probe in an async task which calls the given callback once with the results of
     * the probe as input.
     * @param callback The callback called once the HTTP probe has finished. The input to the
     * callback is the result of the probe as a |ProbeResult|.
     */
    private void sendHttpProbe(final Callback<Integer> callback) {
        // Gets the HTTP parameters from Prefs. Uses the default value if no value in prefs.
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        String userAgentString = sharedPreferencesManager.readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_USER_AGENT_STRING, "");
        String httpProbeUrl = sharedPreferencesManager.readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_URL, DEFAULT_HTTP_PROBE_URL);
        int httpProbeTimeoutMs = sharedPreferencesManager.readInt(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_TIMEOUT_MS,
                DEFAULT_HTTP_PROBE_TIMEOUT_MS);
        String httpProbeMethod = sharedPreferencesManager.readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_METHOD,
                DEFAULT_HTTP_PROBE_METHOD);

        mHttpProbeAsyncTask = new AsyncTask<Integer>() {
            @Override
            protected Integer doInBackground() {
                HttpURLConnection urlConnection = null;
                try {
                    URL url = new URL(httpProbeUrl);
                    urlConnection = (HttpURLConnection) ChromiumNetworkAdapter.openConnection(
                            url, NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
                    urlConnection.setInstanceFollowRedirects(false);
                    urlConnection.setRequestMethod(httpProbeMethod);
                    urlConnection.setConnectTimeout(httpProbeTimeoutMs);
                    urlConnection.setReadTimeout(httpProbeTimeoutMs);
                    urlConnection.setUseCaches(false);
                    urlConnection.setRequestProperty(USER_AGENT_HEADER_NAME, userAgentString);

                    urlConnection.connect();
                    int responseCode = urlConnection.getResponseCode();

                    if (responseCode == HttpURLConnection.HTTP_NO_CONTENT) {
                        // Validated the connection with no content.
                        return ProbeResult.VALIDATED;
                    }

                    if (responseCode >= 400) {
                        // There is still needs to be a connection in order to receive a server
                        // error response.
                        return ProbeResult.SERVER_ERROR;
                    }
                } catch (IOException e) {
                    // Most likely the exception is thrown due to host name not resolved or socket
                    // timeout.
                    return ProbeResult.NO_INTERNET;
                } catch (ArrayIndexOutOfBoundsException | NullPointerException e) {
                    // Most likely these exceptions were thrown due to two HttpURLConnections
                    // running at the same time.
                    return ProbeResult.MULTIPLE_URL_CONNECTIONS_OPEN;
                } finally {
                    if (urlConnection != null) {
                        urlConnection.disconnect();
                    }
                }
                // The result doesn't match the expected result. This is likely caused by a captive
                // portal or a broken transparent proxy.
                return ProbeResult.UNEXPECTED_RESPONSE;
            }

            @Override
            protected void onPostExecute(Integer result) {
                callback.onResult(result);
            }
        };
        mHttpProbeAsyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Determines if airplane mode is enabled or disabled currently.
     * @param context The current application context.
     * @return Whether or not airplane mode is currently enabled. If context is null, then false is
     *         returned.
     */
    private static boolean isAirplaneModeEnabled(Context context) {
        if (sIsAirplaneModeEnabledTestingOverride != null) {
            return sIsAirplaneModeEnabledTestingOverride;
        }

        return Settings.Global.getInt(
                       context.getContentResolver(), Settings.Global.AIRPLANE_MODE_ON, 0)
                != 0;
    }

    /**
     * Whether or not the system is currently roaming.
     * @param context the current application context.
     * @return Whether or not all current networks are roaming or not. If at least one network is
     *         not roaming, then false is returned. If context is null, then false is also returned.
     */
    private static boolean isRoaming(Context context) {
        if (sIsRoamingTestingOverride != null) {
            return sIsRoamingTestingOverride;
        }

        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        Network[] allNetworks = connectivityManager.getAllNetworks();

        // If there are no networks, then the system is completely offline. We consider this as not
        // roaming.
        if (allNetworks.length == 0) {
            return false;
        }

        // If and only if all networks are roaming, then the system is roaming.
        for (Network network : allNetworks) {
            NetworkCapabilities networkCapabilities =
                    connectivityManager.getNetworkCapabilities(network);
            if (networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING)) {
                return false;
            }
        }
        return true;
    }

    private static boolean isInteractive(Context context) {
        if (sIsInteractiveTestingOverride != null) {
            return sIsInteractiveTestingOverride;
        }

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return powerManager.isInteractive();
    }

    private static boolean isApplicationForeground() {
        if (sIsApplicationForegroundTestingOverride != null) {
            return sIsApplicationForegroundTestingOverride;
        }

        return ApplicationStatus.getStateForApplication()
                == ApplicationState.HAS_RUNNING_ACTIVITIES;
    }

    @VisibleForTesting
    static void setIsAirplaneModeEnabledForTesting(boolean isAirplaneModeEnabled) {
        sIsAirplaneModeEnabledTestingOverride = isAirplaneModeEnabled;
    }

    @VisibleForTesting
    static void setIsRoamingForTesting(boolean isRoaming) {
        sIsRoamingTestingOverride = isRoaming;
    }

    @VisibleForTesting
    static void setIsInteractiveForTesting(boolean isInteractive) {
        sIsInteractiveTestingOverride = isInteractive;
    }

    @VisibleForTesting
    static void setIsApplicationForegroundForTesting(boolean isApplicationForeground) {
        sIsApplicationForegroundTestingOverride = isApplicationForeground;
    }

    private static int convertToUserState(boolean didSystemBootSinceLastCheck,
            boolean isInteractive, boolean isApplicationForeground) {
        if (didSystemBootSinceLastCheck) {
            return UserState.PHONE_OFF;
        }
        if (!isInteractive) {
            return UserState.NOT_USING_PHONE;
        }
        if (isApplicationForeground) {
            return UserState.USING_CHROME;
        }
        return UserState.USING_PHONE_NOT_CHROME;
    }

    private static String getSystemStateListFromPrefsAsString() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST, "");
    }

    private static void addSystemStateToListInPrefs(SystemState systemState) {
        SystemStateList systemStateList = getSystemStateListFromPrefs();
        systemStateList =
                SystemStateList.newBuilder(systemStateList).addSystemStates(systemState).build();
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST,
                Base64.encodeToString(systemStateList.toByteArray(), Base64.DEFAULT));
    }

    private static SystemStateList getSystemStateListFromPrefs() {
        String rawList = getSystemStateListFromPrefsAsString();
        try {
            return SystemStateList.parseFrom(Base64.decode(rawList, Base64.DEFAULT));
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            // If we can't parse the system state list from prefs, return a list with one invalid
            // entry.
            SystemState invalidSystemState =
                    SystemState.newBuilder()
                            .setUserState(SystemState.UserState.INVALID_USER_STATE)
                            .setProbeResult(SystemState.ProbeResult.INVALID_PROBE_RESULT)
                            .build();
            return SystemStateList.newBuilder().addSystemStates(invalidSystemState).build();
        }
    }

    private static void clearSystemStateListFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_SYSTEM_STATE_LIST);
    }
}
