// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offline.measurements;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.provider.Settings;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.HttpURLConnection;
import java.net.URL;
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

    // The result of the HTTP probing. Defined in tools/metrics/histograms/enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({ProbeResult.NO_INTERNET, ProbeResult.SERVER_ERROR, ProbeResult.UNEXPECTED_RESPONSE,
            ProbeResult.VALIDATED, ProbeResult.CANCELLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProbeResult {
        // Value could not be prased from Prefs.
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
        // Count.
        int RESULT_COUNT = 6;
    }

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

    // Runs the HTTP probe.
    private AsyncTask<Integer> mHttpProbeAsyncTask;

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

        int[] httpProbeResultList = getHttpProbeResultsFromPrefs();
        for (int httpProbeResult : httpProbeResultList) {
            RecordHistogram.recordEnumeratedHistogram(OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULT,
                    httpProbeResult, ProbeResult.RESULT_COUNT);
        }

        boolean[] isAirplaneModeEnabledList = getIsAirplaneModeEnabledListFromPrefs();
        for (boolean isAirplaneModeEnabled : isAirplaneModeEnabledList) {
            RecordHistogram.recordBooleanHistogram(
                    OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED, isAirplaneModeEnabled);
        }

        boolean[] isRoamingList = getIsRoamingListFromPrefs();
        for (boolean isRoaming : isRoamingList) {
            RecordHistogram.recordBooleanHistogram(OFFLINE_MEASUREMENTS_IS_ROAMING, isRoaming);
        }

        // After logging the data to UMA, clear the data from prefs so it isn't logged again.
        clearTimeBetweenChecksFromPrefs();
        clearHttpProbeResultsFromPrefs();
        clearIsAirplaneModeEnabledListFromPrefs();
        clearIsRoamingListFromPrefs();
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
        // match the default value, then it is written to Prefs..
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

        if (lastCheckMillis > 0) {
            long timeBetweenChecksMillis = currentCheckMillis - lastCheckMillis;
            addTimeBetweenChecksToPrefs(timeBetweenChecksMillis);
        }

        // Gets whether airplane mode is enabled or disabled.
        boolean isAirplaneModeEnabled = isAirplaneModeEnabled(context);
        boolean isRoaming = isRoaming(context);
        addIsAirplaneModeEnabledToPrefs(isAirplaneModeEnabled);
        addIsRoamingToPrefs(isRoaming);

        // Starts the HTTP probe.
        sendHttpProbe((Integer result) -> { processResult(result, callback); });

        return true;
    }

    /**
     * Saves the result of the HTTP probe to Prefs, and informs the task scheduler that the task is
     * finished.
     * @param result The result of the HTTP probe as a |ProbeResult|.
     * @param callback The callback used to inform the background task scheduler that the task has
     * finished.
     */
    private void processResult(Integer result, TaskFinishedCallback callback) {
        // Save the result of the HTTP probe to Prefs, so that it can be recorded to UMA
        // later.
        addHttpProbeResultToPrefs(result);

        // TODO(curranmax): Convert the result to a boolean of whether the system is online or
        // offline, then write this (along with other information) to UKM. https://crbug.com/1131600

        // Informs scheduler that the background task has finished.
        callback.taskFinished(false);
    }

    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // Cancels the HTTP probe if it is still running.
        if (mHttpProbeAsyncTask != null) {
            mHttpProbeAsyncTask.cancel(true);
            addHttpProbeResultToPrefs(ProbeResult.CANCELLED);
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
                    urlConnection = (HttpURLConnection) url.openConnection();
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
                    connectivityManager.getNetworkCapabilities(allNetworks[0]);
            if (networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING)) {
                return false;
            }
        }
        return true;
    }

    @VisibleForTesting
    static void setIsAirplaneModeEnabledForTesting(boolean isAirplaneModeEnabled) {
        sIsAirplaneModeEnabledTestingOverride = isAirplaneModeEnabled;
    }

    @VisibleForTesting
    static void setIsRoamingForTesting(boolean isRoaming) {
        sIsRoamingTestingOverride = isRoaming;
    }

    private static String getTimeBetweenChecksFromPrefsAsString() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST, "");
    }

    private static void addTimeBetweenChecksToPrefs(long newValue) {
        // Add new value to comma separate list currently in Prefs.
        String existingList = getTimeBetweenChecksFromPrefsAsString();
        String newList = addValueToStringList(newValue, existingList);

        // Write the new list to Prefs.
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST, newList);
    }

    private static long[] getTimeBetweenChecksFromPrefs() {
        String rawList = getTimeBetweenChecksFromPrefsAsString();
        return getValuesFromStringList(rawList, -1);
    }

    private static void clearTimeBetweenChecksFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_TIME_BETWEEN_CHECKS_MILLIS_LIST);
    }

    private static String getHttpProbeResultsFromPrefsAsString() {
        String rv = SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULTS_LIST, "");
        return rv;
    }

    private static void addHttpProbeResultToPrefs(int newValue) {
        // Add new value to comma separate list currently in Prefs.
        String existingList = getHttpProbeResultsFromPrefsAsString();
        String newList = addValueToStringList(newValue, existingList);

        // Write the new list to Prefs.
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULTS_LIST, newList);
    }

    private static int[] getHttpProbeResultsFromPrefs() {
        // Get values as an array of longs.
        String rawList = getHttpProbeResultsFromPrefsAsString();
        long[] valuesAsLongs = getValuesFromStringList(rawList, ProbeResult.INVALID);

        // Convert each element to ints.
        int[] valuesAsInts = new int[valuesAsLongs.length];
        for (int i = 0; i < valuesAsLongs.length; i++) {
            valuesAsInts[i] = (int) valuesAsLongs[i];
        }
        return valuesAsInts;
    }

    private static void clearHttpProbeResultsFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_HTTP_PROBE_RESULTS_LIST);
    }

    private static String getIsAirplaneModeEnabledListFromPrefsAsString() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED_LIST, "");
    }

    private static void addIsAirplaneModeEnabledToPrefs(boolean isAirplaneModeEnabled) {
        // Add the value to the list.
        String existingList = getIsAirplaneModeEnabledListFromPrefsAsString();
        String newList = addValueToStringList(isAirplaneModeEnabled ? 1 : 0, existingList);

        // Write the new list to Prefs
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED_LIST, newList);
    }

    private static boolean[] getIsAirplaneModeEnabledListFromPrefs() {
        // Get values as an array of longs.
        String rawList = getIsAirplaneModeEnabledListFromPrefsAsString();
        long[] valuesAsLongs = getValuesFromStringList(rawList, 0);

        // Convert each element to boolean.
        boolean[] valuesAsBools = new boolean[valuesAsLongs.length];
        for (int i = 0; i < valuesAsLongs.length; i++) {
            valuesAsBools[i] = valuesAsLongs[i] != 0;
        }
        return valuesAsBools;
    }

    private static void clearIsAirplaneModeEnabledListFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_IS_AIRPLANE_MODE_ENABLED_LIST);
    }

    private static String getIsRoamingListFromPrefsAsString() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_IS_ROAMING_LIST, "");
    }

    private static void addIsRoamingToPrefs(boolean isRoaming) {
        // Add the value to the list.
        String existingList = getIsRoamingListFromPrefsAsString();
        String newList = addValueToStringList(isRoaming ? 1 : 0, existingList);

        // Write the new list to Prefs
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_IS_ROAMING_LIST, newList);
    }

    private static boolean[] getIsRoamingListFromPrefs() {
        // Get values as an array of longs.
        String rawList = getIsRoamingListFromPrefsAsString();
        long[] valuesAsLongs = getValuesFromStringList(rawList, 0);

        // Convert each element to boolean.
        boolean[] valuesAsBools = new boolean[valuesAsLongs.length];
        for (int i = 0; i < valuesAsLongs.length; i++) {
            valuesAsBools[i] = valuesAsLongs[i] != 0;
        }
        return valuesAsBools;
    }

    private static void clearIsRoamingListFromPrefs() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.OFFLINE_MEASUREMENTS_IS_ROAMING_LIST);
    }

    /**
     * Adds the given value to the end of the comma separated list.
     * @param newValue Value to be added to the end of the list.
     * @param rawList Comma separated list.
     * @return A comma separated list made up of |rawList| with |newValue| appended to the end.
     */
    private static String addValueToStringList(long newValue, String rawList) {
        StringBuilder strBuilder = new StringBuilder(rawList);
        if (strBuilder.length() > 0) {
            strBuilder.append(",");
        }
        strBuilder.append(newValue);

        return strBuilder.toString();
    }

    /**
     * Parses the given comma separated list and converts the elements to longs.
     * @param rawList Comma separated list.
     * @param defaultValue The value used if an element cannot be parsed to a long.
     * @return The input array split by commas and converted to longs.
     */
    private static long[] getValuesFromStringList(String rawList, long defaultValue) {
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
                longAsLongs[i] = defaultValue;
            }
        }
        return longAsLongs;
    }
}
