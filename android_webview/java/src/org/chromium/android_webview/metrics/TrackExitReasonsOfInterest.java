// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.metrics;

import android.os.Build;
import android.os.Process;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
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
import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

/**
 * Tracks the most recent exit reason of interest in embedding WebView apps
 * associated with WebView's last pid.
 *
 * @return The most recent exit reason of interest matching the last PID.
 */
@RequiresApi(Build.VERSION_CODES.R)
@JNINamespace("android_webview")
public class TrackExitReasonsOfInterest {
    public static final String TAG = "TrackExitReasons";
    public static final String LAST_EXIT_INFO_FILENAME = "last-exit-info";
    public static final String LAST_EXIT_INFO_PID = "exitInfoPid";
    public static final String TIMESTAMP_AT_LAST_RECORDING_IN_MILLIS =
            "timestampAtLastRecordingInMillis";
    public static final String LAST_STATE_KEY = "appState";
    public static final String UMA_DELTA = "Android.WebView.HistoricalApplicationExitInfo.Delta";
    public static final String UMA_COUNTS = "Android.WebView.HistoricalApplicationExitInfo.Counts";
    private static long sTestTime;
    private static int sPid;

    @VisibleForTesting public static final Map<Integer, String> sUmaSuffixMap = createMap();

    private static Map<Integer, String> createMap() {
        Map<Integer, String> umaSuffixMap = new HashMap<Integer, String>();
        umaSuffixMap.put(AppState.UNKNOWN, "UNKNOWN");
        umaSuffixMap.put(AppState.FOREGROUND, "FOREGROUND");
        umaSuffixMap.put(AppState.BACKGROUND, "BACKGROUND");
        umaSuffixMap.put(AppState.DESTROYED, "DESTROYED");
        return umaSuffixMap;
    }

    private static long sTimestampAtLastRecording;
    // To avoid any potential synchronization issues we post all exit reason actions to
    // the same sequence to be run serially. This should also guarantee that the #run method
    // always runs prior to any #writeLastWebViewState invocations, since the run method is
    // called at startup.
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);
    private static Supplier<Integer> sAppStateSupplier;
    private static @AppState int sLastStateWritten;

    /** Posts the exit reason tracker work in task runner queue. */
    public static void init(Supplier<Integer> stateSupplier) {
        sAppStateSupplier = stateSupplier;
        sSequencedTaskRunner.execute(
                () -> {
                    run();
                });
    }

    /**
     *
     * @return The latest exit reason matching the last process's PID
     */
    @VisibleForTesting
    public static int run() {
        ExitReasonData previousInfoData = readLastExitInfo();
        int pid = sPid != 0 ? sPid : Process.myPid();
        // No previous pid, so no need to attempt to log anything
        // Return -1 for testing purposes
        int systemExitReason = -1;

        sTimestampAtLastRecording = currentTimeMillis();
        if (previousInfoData != null) {
            systemExitReason =
                    ProcessExitReasonFromSystem.getExitReason(previousInfoData.mExitInfoPid);
            Integer mappedExitReasonData =
                    ProcessExitReasonFromSystem.convertApplicationExitInfoToExitReason(
                            systemExitReason);

            // Log the new record
            if (mappedExitReasonData != null) {
                if (previousInfoData.mTimestampAtLastRecordingInMillis > 0L) {
                    long delta =
                            sTimestampAtLastRecording
                                    - previousInfoData.mTimestampAtLastRecordingInMillis;
                    RecordHistogram.recordLongTimesHistogram100(UMA_DELTA, delta);
                }
                ProcessExitReasonFromSystem.recordAsEnumHistogram(UMA_COUNTS, systemExitReason);
                ProcessExitReasonFromSystem.recordAsEnumHistogram(
                        UMA_COUNTS + "." + sUmaSuffixMap.get(previousInfoData.mState),
                        systemExitReason);
            }
        }
        writeLastExitInfo(
                new ExitReasonData(pid, sTimestampAtLastRecording, sAppStateSupplier.get()));

        return systemExitReason;
    }

    /** Data class for tracking exit reasons */
    public static class ExitReasonData {
        public final int mExitInfoPid;
        public final long mTimestampAtLastRecordingInMillis;
        public final @AppState int mState;

        public ExitReasonData(int pid, long timestampAtLastRecordingInMillis, @AppState int state) {
            mExitInfoPid = pid;
            mTimestampAtLastRecordingInMillis = timestampAtLastRecordingInMillis;
            mState = state;
        }
    }

    @VisibleForTesting
    public static ExitReasonData readLastExitInfo() {
        try (FileInputStream fis = new FileInputStream(getLastExitInfoFile())) {
            String buffer = new String(FileUtils.readStream(fis));
            JSONObject jsonObj = new JSONObject(buffer);

            // Want to early return since we cannot do anything useful without previous PID
            if (!jsonObj.has(LAST_EXIT_INFO_PID)) return null;
            int exitInfoPid = jsonObj.getInt(LAST_EXIT_INFO_PID);

            long timestampAtLastRecordingInMillis =
                    jsonObj.optLong(TIMESTAMP_AT_LAST_RECORDING_IN_MILLIS);
            @AppState int state = jsonObj.optInt(LAST_STATE_KEY);

            return new ExitReasonData(exitInfoPid, timestampAtLastRecordingInMillis, state);
        } catch (IOException e) {
            // File does not exist or the file read fails here
            return null;
        } catch (JSONException e) {
            Log.i(TAG, "Failed to parse JSON from file.");
        }

        return null;
    }

    /**
     * This stores the exit info data for future runs. The {@link
     * org.chromium.android_webview.AwContentsLifecycleNotifier} will also call this when users
     * change apps between the foreground & background.
     *
     * @param newData Provides the pid and timestamp at last recording
     * @return whether the newData is valid
     */
    @VisibleForTesting
    public static boolean writeLastExitInfo(final ExitReasonData newData) {
        return writeLastExitInfo(newData, /* callbackResult= */ null);
    }

    @VisibleForTesting
    public static boolean writeLastExitInfo(
            final ExitReasonData newData, final Callback<Boolean> callbackResult) {
        try (FileOutputStream writer =
                new FileOutputStream(getLastExitInfoFile(), /* append= */ false)) {
            JSONObject jsonObj = new JSONObject();
            jsonObj.put(LAST_EXIT_INFO_PID, newData.mExitInfoPid);
            jsonObj.put(
                    TIMESTAMP_AT_LAST_RECORDING_IN_MILLIS,
                    newData.mTimestampAtLastRecordingInMillis);
            jsonObj.put(LAST_STATE_KEY, newData.mState);
            String jsonString = jsonObj.toString();
            writer.write(jsonString.getBytes());
            writer.flush();
        } catch (JSONException | IOException e) {
            Log.e(TAG, "Failed to write last exit info.");
            if (callbackResult != null) {
                callbackResult.onResult(false);
            }
            return false;
        }

        sLastStateWritten = newData.mState;
        if (callbackResult != null) {
            callbackResult.onResult(true);
        }
        return true;
    }

    @VisibleForTesting
    public static File getLastExitInfoFile() {
        return new File(PathUtils.getDataDirectory(), LAST_EXIT_INFO_FILENAME);
    }

    /** Commits the latest app state for exit reason tracking to disk. */
    public static void writeLastWebViewState() {
        writeLastWebViewState(/* callbackResult= */ null);
    }

    /**
     * Commits the latest app state for exit reason tracking to disk.
     *
     * @param callbackResult Used for testing purposes only for waiting on disk writes to complete.
     */
    @VisibleForTesting
    public static void writeLastWebViewState(final Callback<Boolean> callbackResult) {
        int pid = sPid != 0 ? sPid : Process.myPid();
        sSequencedTaskRunner.execute(
                () -> {
                    @AppState int currentState = sAppStateSupplier.get();
                    if (sLastStateWritten != currentState) {
                        writeLastExitInfo(
                                new ExitReasonData(pid, sTimestampAtLastRecording, currentState),
                                callbackResult);
                    }
                });
    }

    private static long currentTimeMillis() {
        if (sTestTime > 0L) return sTestTime;
        return System.currentTimeMillis();
    }

    @VisibleForTesting
    public static void setSystemTimeForTest(long testTime) {
        sTestTime = testTime;
    }

    @VisibleForTesting
    public static void setPidForTest(int pid) {
        sPid = pid;
    }

    @VisibleForTesting
    public static void setCurrtimeForTest(long currTime) {
        sTimestampAtLastRecording = currTime;
    }

    @VisibleForTesting
    public static void setStateSupplier(Supplier<Integer> supplier) {
        sAppStateSupplier = supplier;
    }
}
