// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.metrics;

import android.os.Build;
import android.os.Process;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.android_webview.AppState;
import org.chromium.base.Callback;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/** Tracks and logs the most recent exit reasons in embedding WebView apps. */
@RequiresApi(Build.VERSION_CODES.R)
@JNINamespace("android_webview")
public class TrackExitReasons {
    private static final String TAG = "TrackExitReasons";
    private static final String FILE_NAME = "last-exit-info";
    private static final String KEY_PID = "pid";
    private static final String KEY_TIME_MILLIS = "timeMillis";
    private static final String KEY_STATE = "appState";
    private static final String KEY_DATA_ARRAY = "dataArray";
    private static final String KEY_VERSION = "version";
    private static final int FILE_VERSION = 2;
    // The maximum number of data entries to store in the file, as we don't want the file to grow
    // unbounded. Also, the OS has the same limit per app for storing exit reasons, so there is no
    // point in storing more.
    public static final int MAX_DATA_LIST_SIZE = 16;
    public static final String UMA_DELTA = "Android.WebView.HistoricalApplicationExitInfo.Delta2";
    public static final String UMA_COUNTS = "Android.WebView.HistoricalApplicationExitInfo.Counts2";
    private static long sCurrentTimeMillisForTest;

    // The id of the current process.
    private static int sPid;

    @VisibleForTesting
    public static final Map<Integer, String> sUmaSuffixMap =
            Map.of(
                    AppState.UNKNOWN, "UNKNOWN",
                    AppState.FOREGROUND, "FOREGROUND",
                    AppState.BACKGROUND, "BACKGROUND",
                    AppState.DESTROYED, "DESTROYED",
                    AppState.STARTUP, "STARTUP");

    // To avoid any potential synchronization issues and avoid burdening the UI thread, we post all
    // file reads and writes to the same sequence to be run serially.
    private static final TaskRunner sTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);
    // The app state supplier.
    private static Supplier<Integer> sStateSupplier;
    private static int sLastStateWritten = -1;

    /**
     * Looks up the exit reason for the process identified in the data, and records histograms in
     * UMA if the reason is found.
     *
     * @return The exit reason or null if not found
     */
    @VisibleForTesting
    public static int findExitReasonAndLog(AppStateData data) {
        int systemReason = ProcessExitReasonFromSystem.getExitReason(data.mPid);
        Integer convertedReason = ProcessExitReasonFromSystem.convertToExitReason(systemReason);

        // If the converted reason is null, the system reason is unknown and we cannot
        // log it.
        if (convertedReason == null) return -1;

        if (data.mTimeMillis > 0L) {
            long delta = getCurrentTimeMillis() - data.mTimeMillis;
            RecordHistogram.recordLongTimesHistogram100(UMA_DELTA, delta);
        }

        ProcessExitReasonFromSystem.recordAsEnumHistogram(UMA_COUNTS, systemReason);
        ProcessExitReasonFromSystem.recordAsEnumHistogram(
                UMA_COUNTS + "." + sUmaSuffixMap.get(data.mState), systemReason);

        return systemReason;
    }

    /** Data class for holding app state and related details. */
    public static class AppStateData {
        // The process id.
        public final int mPid;

        // The time in milliseconds since the epoch.
        public final long mTimeMillis;

        // The app state.
        public final @AppState int mState;

        public AppStateData(int pid, long timeMillis, @AppState int state) {
            mPid = pid;
            mTimeMillis = timeMillis;
            mState = state;
        }
    }

    /**
     * Reads the list of {@link AppStateData} from a file.
     *
     * @return The list of {@link AppStateData} that was written to the file, empty if no data is
     *     found. One of the data can be about the current process, it will have a matching process
     *     id.
     */
    @VisibleForTesting
    public static List<AppStateData> readData() {
        List<AppStateData> dataList = new ArrayList<>();

        try (FileInputStream fis = new FileInputStream(getFile())) {
            String buffer = new String(FileUtils.readStream(fis));
            JSONObject jsonRoot = new JSONObject(buffer);

            int version = jsonRoot.getInt(KEY_VERSION);
            // If the file format is not for the expected version we cannot proceed.
            if (version != FILE_VERSION) return dataList;

            JSONArray dataArray = jsonRoot.optJSONArray(KEY_DATA_ARRAY);
            for (int i = 0; i < dataArray.length(); i++) {
                JSONObject data = dataArray.getJSONObject(i);
                int pid = data.getInt(KEY_PID);
                long timeMillis = data.getLong(KEY_TIME_MILLIS);
                @AppState int state = data.getInt(KEY_STATE);
                dataList.add(new AppStateData(pid, timeMillis, state));
            }
        } catch (IOException e) {
            // File does not exist or the file read failed.
        } catch (JSONException e) {
            Log.i(TAG, "Failed to parse JSON from file.");
        }

        return dataList;
    }

    /**
     * Writes the app state and the current process id and time to a file, overwriting any existing
     * data.
     *
     * @param state The {@link AppState} to write
     * @return whether the data was written successfully
     */
    public static boolean writeState(@AppState int state) {
        return writeState(state, null);
    }

    /**
     * Writes the app state and the current process id and time to a file, overwriting any existing
     * data.
     *
     * @param state The {@link AppState} to write
     * @return whether the data was written successfully
     */
    @VisibleForTesting
    public static boolean writeState(@AppState int state, Callback<Boolean> resultCallback) {
        sLastStateWritten = state;
        AppStateData data = new AppStateData(getPid(), getCurrentTimeMillis(), state);
        return writeData(List.of(data), resultCallback);
    }

    /**
     * Reads the previous app state data from file, appends the current state, and writes it back.
     */
    private static boolean appendCurrentState(Callback<Boolean> resultCallback) {
        List<AppStateData> oldDataList = readData();
        List<AppStateData> newDataList = new ArrayList<>();
        for (int i = 0; i < oldDataList.size() && i < MAX_DATA_LIST_SIZE - 1; i++) {
            // During startup we do not expect to find data with the same pid because we're spinning
            // up a new process with a new pid, but a collision with a previous process id could
            // happen very rarely.
            if (oldDataList.get(i).mPid != getPid()) newDataList.add(oldDataList.get(i));
        }
        newDataList.add(new AppStateData(getPid(), getCurrentTimeMillis(), sStateSupplier.get()));
        sLastStateWritten = sStateSupplier.get();
        return writeData(newDataList, resultCallback);
    }

    /**
     * Writes the data to a file, overwriting any existing data.
     *
     * @param data The data to write
     * @return whether the data was written successfully
     */
    @VisibleForTesting
    public static boolean writeData(List<AppStateData> dataList, Callback<Boolean> resultCallback) {
        try (FileOutputStream writer = new FileOutputStream(getFile())) {
            JSONObject jsonRoot = new JSONObject();
            jsonRoot.put(KEY_VERSION, FILE_VERSION);

            List<JSONObject> jsonDataList = new ArrayList<>();
            for (AppStateData data : dataList) {
                JSONObject jsonData = new JSONObject();
                jsonData.put(KEY_PID, data.mPid);
                jsonData.put(KEY_TIME_MILLIS, data.mTimeMillis);
                jsonData.put(KEY_STATE, data.mState);
                jsonDataList.add(jsonData);
            }
            jsonRoot.put(KEY_DATA_ARRAY, new JSONArray(jsonDataList));

            writer.write(jsonRoot.toString().getBytes());
            writer.flush();
        } catch (JSONException | IOException e) {
            Log.e(TAG, "Failed to write file.");
            if (resultCallback != null) resultCallback.onResult(false);
            return false;
        }

        if (resultCallback != null) resultCallback.onResult(true);
        return true;
    }

    /** Gets the app state from the supplier and writes it to a file if it has changed. */
    public static void updateAppState() {
        updateAppState(null);
    }

    /** Gets the app state from the supplier and writes it to a file if it has changed. */
    @VisibleForTesting
    public static void updateAppState(Callback<Boolean> resultCallback) {
        sTaskRunner.execute(
                () -> {
                    @AppState int currentState = sStateSupplier.get();
                    if (sLastStateWritten != currentState) {
                        writeState(currentState, resultCallback);
                    } else {
                        if (resultCallback != null) resultCallback.onResult(false);
                    }
                });
    }

    /** Starts tracking app state at startup for the current process. */
    public static void startTrackingStartup() {
        startTrackingStartup(null);
    }

    /** Starts tracking app state at startup for the current process. */
    @VisibleForTesting
    public static void startTrackingStartup(Callback<Boolean> resultCallback) {
        sTaskRunner.execute(
                () -> {
                    setStateSupplier(
                            () -> {
                                return AppState.STARTUP;
                            });
                    appendCurrentState(resultCallback);
                });
    }

    public static void finishTrackingStartup(Supplier<Integer> stateSupplier) {
        finishTrackingStartup(stateSupplier, null);
    }

    public static void finishTrackingStartup(
            Supplier<Integer> stateSupplier, Callback<Boolean> resultCallback) {
        sTaskRunner.execute(
                () -> {
                    setStateSupplier(stateSupplier);
                    for (AppStateData data : readData()) {
                        // Log app state and exit reason data about previous processes. There could
                        // be data for multiple processes if early exits in previous runs prevented
                        // us from logging it and clearing it from the file.
                        if (data.mPid != getPid()) findExitReasonAndLog(data);
                    }
                    writeState(sStateSupplier.get(), resultCallback);
                });
    }

    @VisibleForTesting
    public static File getFile() {
        return new File(PathUtils.getDataDirectory(), FILE_NAME);
    }

    private static long getCurrentTimeMillis() {
        if (sCurrentTimeMillisForTest > 0L) return sCurrentTimeMillisForTest;
        return System.currentTimeMillis();
    }

    public static void setCurrentTimeMillisForTest(long timeMillis) {
        sCurrentTimeMillisForTest = timeMillis;
    }

    private static int getPid() {
        return sPid != 0 ? sPid : Process.myPid();
    }

    public static void setPidForTest(int pid) {
        sPid = pid;
    }

    @VisibleForTesting
    public static void setStateSupplier(Supplier<Integer> stateSupplier) {
        sStateSupplier = stateSupplier;
    }
}
