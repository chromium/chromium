// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.crash.LogcatCrashExtractor;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;

/**
 * Extracts the recent logcat output from an Android device, elides PII sensitive info from it,
 * prepends the logcat data to the caller-provided minidump file, and initiates upload for the crash
 * report.
 */
public class LogcatExtractionRunnable implements Runnable {
    private static final String TAG = "LogcatExtraction";

    private final File mMinidumpFile;
    private final LogcatCrashExtractor mLogcatExtractor;

    /**
     * @param minidump The minidump file that needs logcat output to be attached.
     */
    public LogcatExtractionRunnable(File minidump) {
        this(minidump, new LogcatCrashExtractor());
    }

    /**
     * @param minidump The minidump file that needs logcat output to be attached.
     * @param logcatExtractor to allow injecting extractor for testing.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected LogcatExtractionRunnable(File minidump, LogcatCrashExtractor logcatExtractor) {
        mMinidumpFile = minidump;
        mLogcatExtractor = logcatExtractor;
    }

    @Override
    public void run() {
        File fileToUpload =
                mLogcatExtractor.attachLogcatToMinidump(
                        mMinidumpFile,
                        new CrashFileManager(ContextUtils.getApplicationContext().getCacheDir()));
        uploadMinidump(fileToUpload, false);
    }

    /**
     * @param minidump the minidump file to be uploaded.
     * @param uploadNow If this flag is set to true, we will upload the minidump immediately,
     * otherwise the upload is controlled by the job scheduler.
     */
    /* package */ static void uploadMinidump(File minidump, boolean uploadNow) {
        // Regardless of success, initiate the upload. That way, even if there are errors augmenting
        // the minidump with logcat data, the service can still upload the unaugmented minidump.
        try {
            if (uploadNow) {
                MinidumpUploadServiceImpl.tryUploadCrashDumpNow(minidump);
            } else {
                MinidumpUploadServiceImpl.scheduleUploadJob();
            }
        } catch (SecurityException e) {
            Log.w(TAG, e.toString());
            if (!uploadNow) throw e;
        }
    }
}
