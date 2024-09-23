// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.app.job.JobInfo;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Build;
import android.os.PersistableBundle;
import android.os.Process;

import androidx.annotation.StringDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.crash.browser.ProcessExitReasonFromSystem;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable.MinidumpUploadStatus;
import org.chromium.components.minidump_uploader.MinidumpUploadJobService;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.atomic.AtomicBoolean;

/** Service that is responsible for uploading crash minidumps to the Google crash server. */
public class MinidumpUploadServiceImpl extends MinidumpUploadService.Impl {
    private static final String TAG = "MinidmpUploadService";

    // Intent actions
    @VisibleForTesting
    static final String ACTION_UPLOAD = "com.google.android.apps.chrome.crash.ACTION_UPLOAD";

    // Intent bundle keys
    @VisibleForTesting static final String FILE_TO_UPLOAD_KEY = "minidump_file";
    static final String UPLOAD_LOG_KEY = "upload_log";

    /** The number of times we will try to upload a crash. */
    public static final int MAX_TRIES_ALLOWED = 3;

    /** Histogram related constants. */
    private static final String HISTOGRAM_NAME_PREFIX = "Tab.AndroidCrashUpload_";

    private static final int HISTOGRAM_MAX = 2;
    private static final int FAILURE = 0;
    private static final int SUCCESS = 1;

    private static AtomicBoolean sBrowserCrashMetricsInitialized = new AtomicBoolean();
    private static AtomicBoolean sDidBrowserCrashRecently = new AtomicBoolean();

    @StringDef({ProcessType.BROWSER, ProcessType.RENDERER, ProcessType.GPU, ProcessType.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProcessType {
        String BROWSER = "Browser";
        String RENDERER = "Renderer";
        String GPU = "GPU";
        String OTHER = "Other";
    }

    static final String[] TYPES = {
        ProcessType.BROWSER, ProcessType.RENDERER, ProcessType.GPU, ProcessType.OTHER
    };

    @Override
    protected void onServiceSet() {
        getService().setIntentRedelivery(true);
    }

    /** Schedules uploading of all pending minidumps, using the JobScheduler API. */
    public static void scheduleUploadJob() {
        CrashReportingPermissionManager permissionManager =
                PrivacyPreferencesManagerImpl.getInstance();
        PersistableBundle permissions = new PersistableBundle();
        permissions.putBoolean(
                ChromeMinidumpUploaderDelegate.IS_CLIENT_IN_SAMPLE_FOR_CRASHES,
                permissionManager.isClientInSampleForCrashes());
        permissions.putBoolean(
                ChromeMinidumpUploaderDelegate.IS_UPLOAD_ENABLED_FOR_TESTS,
                permissionManager.isUploadEnabledForTests());

        JobInfo.Builder builder =
                new JobInfo.Builder(
                                TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID,
                                new ComponentName(
                                        ContextUtils.getApplicationContext(),
                                        ChromeMinidumpUploadJobService.class))
                        .setExtras(permissions)
                        .setRequiredNetworkType(JobInfo.NETWORK_TYPE_UNMETERED);
        MinidumpUploadJobService.scheduleUpload(builder);
    }

    private static ApplicationStateListener createApplicationStateListener() {
        return newState -> {
            ChromeSharedPreferences.getInstance()
                    .writeInt(ChromePreferenceKeys.LAST_SESSION_APPLICATION_STATE, newState);
        };
    }

    /** Stores the successes and failures from uploading crash to UMA, */
    public static void storeBreakpadUploadStatsInUma(CrashUploadCountStore pref) {
        sBrowserCrashMetricsInitialized.set(true);

        SharedPreferencesManager sharedPrefs = ChromeSharedPreferences.getInstance();
        int previousPid = sharedPrefs.readInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID);
        @ApplicationState
        int applicationExitState =
                sharedPrefs.readInt(ChromePreferenceKeys.LAST_SESSION_APPLICATION_STATE);
        String umaSuffix;
        if (applicationExitState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            umaSuffix = "Foreground2";
        } else {
            umaSuffix = "Background2";
        }
        sharedPrefs.writeInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_PID, Process.myPid());
        ApplicationStateListener appStateListener = createApplicationStateListener();
        appStateListener.onApplicationStateChange(ApplicationStatus.getStateForApplication());

        if (ThreadUtils.runningOnUiThread()) {
            ApplicationStatus.registerApplicationStateListener(appStateListener);
        } else {
            PostTask.postTask(
                    TaskTraits.UI_BEST_EFFORT,
                    () -> {
                        ApplicationStatus.registerApplicationStateListener(appStateListener);
                    });
        }

        if (previousPid != 0) {
            int reason = ProcessExitReasonFromSystem.getExitReason(previousPid);
            ProcessExitReasonFromSystem.recordAsEnumHistogram(
                    "Stability.Android.SystemExitReason.Browser", reason);
            ProcessExitReasonFromSystem.recordAsEnumHistogram(
                    "Stability.Android.SystemExitReason.Browser." + umaSuffix, reason);
        }

        for (String type : TYPES) {
            for (int success = pref.getCrashSuccessUploadCount(type); success > 0; success--) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME_PREFIX + type, SUCCESS, HISTOGRAM_MAX);
                if (ProcessType.BROWSER.equals(type)) sDidBrowserCrashRecently.set(true);
            }
            for (int fail = pref.getCrashFailureUploadCount(type); fail > 0; fail--) {
                RecordHistogram.recordEnumeratedHistogram(
                        HISTOGRAM_NAME_PREFIX + type, FAILURE, HISTOGRAM_MAX);
                if (ProcessType.BROWSER.equals(type)) sDidBrowserCrashRecently.set(true);
            }

            pref.resetCrashUploadCounts(type);
        }
    }

    /** Returns true if the initial breakpad upload stats have been recorded. */
    @CalledByNative
    private static boolean browserCrashMetricsInitialized() {
        return sBrowserCrashMetricsInitialized.get();
    }

    /**
     * Returns if browser crash dumps were found for recent browser crashes.
     *
     * We detect if the browser crash dump was uploaded in last session (for a previous session) or
     * if a crash dump was seen in current session. Detection of a crash from earlier session is
     * valid right from the point where minidump service was initialized
     * (sBrowserCrashMetricsInitialized() returns true). But, the detection of a crash in previous
     * session is only valid after background minidump upload job is finished, depending on the job
     * scheduler. So, calling this function at startup can return false even if browser crashed in
     * previous session.
     */
    @CalledByNative
    private static boolean didBrowserCrashRecently() {
        assert browserCrashMetricsInitialized();
        return sDidBrowserCrashRecently.get();
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        if (intent == null) return;
        if (!ACTION_UPLOAD.equals(intent.getAction())) {
            Log.w(TAG, "Got unknown action from intent: " + intent.getAction());
            return;
        }

        String minidumpFileName = intent.getStringExtra(FILE_TO_UPLOAD_KEY);
        if (minidumpFileName == null || minidumpFileName.isEmpty()) {
            Log.w(TAG, "Cannot upload crash data since minidump is absent.");
            return;
        }
        File minidumpFile = new File(minidumpFileName);
        if (!minidumpFile.isFile()) {
            Log.w(
                    TAG,
                    "Cannot upload crash data since specified minidump "
                            + minidumpFileName
                            + " is not present.");
            return;
        }
        int tries = CrashFileManager.readAttemptNumber(minidumpFileName);

        // Since we do not rename a file after reaching max number of tries,
        // files that have maxed out tries will NOT reach this.
        if (tries >= MAX_TRIES_ALLOWED || tries < 0) {
            // Reachable only if the file naming is incorrect by current standard.
            // Thus we log an error instead of recording failure to UMA.
            Log.e(
                    TAG,
                    "Giving up on trying to upload "
                            + minidumpFileName
                            + " after failing to read a valid attempt number.");
            return;
        }

        String logfileName = intent.getStringExtra(UPLOAD_LOG_KEY);
        File logfile = new File(logfileName);

        // Try to upload minidump
        MinidumpUploadCallable minidumpUploadCallable =
                createMinidumpUploadCallable(minidumpFile, logfile);
        @MinidumpUploadStatus int uploadStatus = minidumpUploadCallable.call();

        if (uploadStatus == MinidumpUploadStatus.SUCCESS) {
            // Only update UMA stats if an intended and successful upload.
            incrementCrashSuccessUploadCount(minidumpFileName);
        } else if (uploadStatus == MinidumpUploadStatus.FAILURE) {
            // Unable to upload minidump. Incrementing try number and restarting.
            ++tries;
            if (tries == MAX_TRIES_ALLOWED) {
                // Only record failure to UMA after we have maxed out the allotted tries.
                incrementCrashFailureUploadCount(minidumpFileName);
            }

            // Only create another attempt if we have successfully renamed the file.
            String newName = CrashFileManager.tryIncrementAttemptNumber(minidumpFile);
            if (newName != null) {
                if (tries < MAX_TRIES_ALLOWED) {
                    MinidumpUploadServiceImpl.scheduleUploadJob();
                } else {
                    Log.d(
                            TAG,
                            "Giving up on trying to upload "
                                    + minidumpFileName
                                    + "after "
                                    + tries
                                    + " number of tries.");
                }
            } else {
                Log.w(TAG, "Failed to rename minidump " + minidumpFileName);
            }
        }
    }

    /** Get the permission manager, can be overridden for testing. */
    CrashReportingPermissionManager getCrashReportingPermissionManager() {
        return PrivacyPreferencesManagerImpl.getInstance();
    }

    private static String getNewNameAfterSuccessfulUpload(String fileName) {
        return fileName.replace("dmp", "up").replace("forced", "up");
    }

    @ProcessType
    @VisibleForTesting
    protected static String getCrashType(String fileName) {
        // Read file and get the line containing name="ptype".
        BufferedReader fileReader = null;
        try {
            fileReader = new BufferedReader(new FileReader(fileName));
            String line;
            while ((line = fileReader.readLine()) != null) {
                if (line.equals("Content-Disposition: form-data; name=\"ptype\"")) {
                    // Crash type is on the line after the next line.
                    fileReader.readLine();
                    String crashType = fileReader.readLine();
                    if (crashType == null) return ProcessType.OTHER;
                    if (crashType.equals("browser")) return ProcessType.BROWSER;
                    if (crashType.equals("renderer")) return ProcessType.RENDERER;
                    if (crashType.equals("gpu-process")) return ProcessType.GPU;
                    return ProcessType.OTHER;
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "Error while reading crash file %s: %s", fileName, e.toString());
        } finally {
            StreamUtil.closeQuietly(fileReader);
        }
        return ProcessType.OTHER;
    }

    /**
     * Increments the count of successful uploads by 1. Distinguishes between different types of
     * crashes by looking into the file contents. Because this code can execute in a context when
     * the main Chrome activity is no longer running, the counts are stored in shared preferences;
     * they are later read and recorded as metrics by the main Chrome activity.
     * NOTE: This method should be called *after* renaming the file, since renaming occurs as a
     * side-effect of a successful upload.
     * @param originalFilename The name of the successfully uploaded minidump, *prior* to uploading.
     */
    public static void incrementCrashSuccessUploadCount(String originalFilename) {
        final @ProcessType String process_type =
                getCrashType(getNewNameAfterSuccessfulUpload(originalFilename));
        if (ProcessType.BROWSER.equals(process_type)) {
            sDidBrowserCrashRecently.set(true);
        }
        CrashUploadCountStore.getInstance().incrementCrashSuccessUploadCount(process_type);
    }

    /**
     * Increments the count of failed uploads by 1. Distinguishes between different types of crashes
     * by looking into the file contents. Because this code can execute in a context when the main
     * Chrome activity is no longer running, the counts are stored in shared preferences; they are
     * later read and recorded as metrics by the main Chrome activity.
     * NOTE: This method should be called *prior* to renaming the file.
     * @param originalFilename The name of the successfully uploaded minidump, *prior* to uploading.
     */
    public static void incrementCrashFailureUploadCount(String originalFilename) {
        final @ProcessType String process_type = getCrashType(originalFilename);
        if (ProcessType.BROWSER.equals(process_type)) {
            sDidBrowserCrashRecently.set(true);
        }
        CrashUploadCountStore.getInstance().incrementCrashFailureUploadCount(process_type);
    }

    /**
     * Factory method for creating minidump callables.
     *
     * This may be overridden for tests.
     *
     * @param minidumpFile the File to upload.
     * @param logfile the Log file to write to upon successful uploads.
     * @return a new MinidumpUploadCallable.
     */
    @VisibleForTesting
    MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile, logfile, getCrashReportingPermissionManager());
    }

    /**
     * Attempts to upload the specified {@param minidumpFile} directly. If Android doesn't allow a
     * direct upload, then fallback to JobScheduler.
     *
     * Note that the preferred way to upload minidump is only through JobScheduler, use this
     * function if you need to upload it urgently.
     */
    static void tryUploadCrashDumpNow(File minidumpFile) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && !(ApplicationStatus.isInitialized()
                        && ApplicationStatus.hasVisibleActivities())) {
            // If we are on API 31+, Android does not allow us to start services from the
            // background. If we are in the background, then go through the JobScheduler path
            // instead. See crbug.com/1433529 for more details.
            scheduleUploadJob();
            return;
        }
        CrashFileManager fileManager =
                new CrashFileManager(ContextUtils.getApplicationContext().getCacheDir());
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), MinidumpUploadService.class);
        intent.setAction(ACTION_UPLOAD);
        intent.putExtra(FILE_TO_UPLOAD_KEY, minidumpFile.getAbsolutePath());
        intent.putExtra(UPLOAD_LOG_KEY, fileManager.getCrashUploadLogFile().getAbsolutePath());
        ContextUtils.getApplicationContext().startService(intent);
    }

    /**
     * Attempts to upload the crash report with the given local ID.
     *
     * Note that this method is asynchronous. All that is guaranteed is that
     * upload attempts will be enqueued.
     *
     * This method is safe to call from the UI thread.
     *
     * @param localId The local ID of the crash report.
     */
    @CalledByNative
    public static void tryUploadCrashDumpWithLocalId(String localId) {
        if (localId == null || localId.isEmpty()) {
            Log.w(TAG, "Cannot force crash upload since local crash id is absent.");
            return;
        }

        CrashFileManager fileManager =
                new CrashFileManager(ContextUtils.getApplicationContext().getCacheDir());
        File minidumpFile = fileManager.getCrashFileWithLocalId(localId);
        if (minidumpFile == null) {
            Log.w(TAG, "Could not find a crash dump with local ID " + localId);
            return;
        }
        File renamedMinidumpFile = CrashFileManager.trySetForcedUpload(minidumpFile);
        if (renamedMinidumpFile == null) {
            Log.w(TAG, "Could not rename the file " + minidumpFile.getName() + " for re-upload");
            return;
        }

        scheduleUploadJob();
    }
}
