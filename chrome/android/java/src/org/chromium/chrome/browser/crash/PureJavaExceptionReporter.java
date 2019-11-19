// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningAppProcessInfo;
import android.content.Context;
import android.os.Build;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.PiiElider;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.MainDex;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.components.crash.CrashKeys;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicReferenceArray;

/**
 * Creates a crash report and uploads it to crash server if there is a Java exception.
 *
 * This class is written in pure Java, so it can handle exception happens before native is loaded.
 */
@MainDex
public class PureJavaExceptionReporter {
    // report fields, please keep the name sync with MIME blocks in breakpad_linux.cc
    public static final String CHANNEL = "channel";
    public static final String VERSION = "ver";
    public static final String PRODUCT = "prod";
    public static final String ANDROID_BUILD_ID = "android_build_id";
    public static final String ANDROID_BUILD_FP = "android_build_fp";
    public static final String SDK = "sdk";
    public static final String DEVICE = "device";
    public static final String GMS_CORE_VERSION = "gms_core_version";
    public static final String INSTALLER_PACKAGE_NAME = "installer_package_name";
    public static final String ABI_NAME = "abi_name";
    public static final String PACKAGE = "package";
    public static final String MODEL = "model";
    public static final String BRAND = "brand";
    public static final String BOARD = "board";
    public static final String EXCEPTION_INFO = "exception_info";
    public static final String PROCESS_TYPE = "ptype";
    public static final String EARLY_JAVA_EXCEPTION = "early_java_exception";
    public static final String CUSTOM_THEMES = "custom_themes";
    public static final String RESOURCES_VERSION = "resources_version";

    private static final String CRASH_DUMP_DIR = "Crash Reports";
    private static final String FILE_PREFIX = "chromium-browser-minidump-";
    private static final String FILE_SUFFIX = ".dmp";
    private static final String RN = "\r\n";
    private static final String FORM_DATA_MESSAGE = "Content-Disposition: form-data; name=\"";

    protected File mMinidumpFile;
    private FileOutputStream mMinidumpFileStream;
    private final String mLocalId = UUID.randomUUID().toString().replace("-", "").substring(0, 16);
    private final String mBoundary = "------------" + UUID.randomUUID() + RN;

    /**
     * Report and upload the device info and stack trace as if it was a crash. Runs synchronously
     * and results in I/O on the main thread.
     *
     * @param javaException The exception to report.
     */
    public static void reportJavaException(Throwable javaException) {
        PureJavaExceptionReporter reporter = new PureJavaExceptionReporter();
        reporter.createAndUploadReport(javaException);
    }

    @VisibleForTesting
    void createAndUploadReport(Throwable javaException) {
        // It is OK to do IO in main thread when we know there is a crash happens.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            createReport(javaException);
            flushToFile();
            uploadReport();
        }
    }

    @VisibleForTesting
    File getMinidumpFile() {
        return mMinidumpFile;
    }

    private void addPairedString(String messageType, String messageData) {
        addString(mBoundary);
        addString(FORM_DATA_MESSAGE + messageType + "\"");
        addString(RN + RN + messageData + RN);
    }

    private void addString(String s) {
        try {
            mMinidumpFileStream.write(ApiCompatibilityUtils.getBytesUtf8(s));
        } catch (IOException e) {
            // Nothing we can do here.
        }
    }

    @SuppressLint("WrongConstant")
    private void createReport(Throwable javaException) {
        try {
            String minidumpFileName = FILE_PREFIX + mLocalId + FILE_SUFFIX;
            mMinidumpFile = new File(
                    new File(ContextUtils.getApplicationContext().getCacheDir(), CRASH_DUMP_DIR),
                    minidumpFileName);
            mMinidumpFileStream = new FileOutputStream(mMinidumpFile);
        } catch (FileNotFoundException e) {
            mMinidumpFile = null;
            mMinidumpFileStream = null;
            return;
        }
        String processName = detectCurrentProcessName();
        if (processName == null || !processName.contains(":")) {
            processName = "browser";
        }

        BuildInfo buildInfo = BuildInfo.getInstance();
        addPairedString(PRODUCT, "Chrome_Android");
        addPairedString(PROCESS_TYPE, processName);
        addPairedString(DEVICE, Build.DEVICE);
        addPairedString(VERSION, ChromeVersionInfo.getProductVersion());
        addPairedString(CHANNEL, getChannel());
        addPairedString(ANDROID_BUILD_ID, Build.ID);
        addPairedString(MODEL, Build.MODEL);
        addPairedString(BRAND, Build.BRAND);
        addPairedString(BOARD, Build.BOARD);
        addPairedString(ANDROID_BUILD_FP, buildInfo.androidBuildFingerprint);
        addPairedString(SDK, String.valueOf(Build.VERSION.SDK_INT));
        addPairedString(GMS_CORE_VERSION, buildInfo.gmsVersionCode);
        addPairedString(INSTALLER_PACKAGE_NAME, buildInfo.installerPackageName);
        addPairedString(ABI_NAME, buildInfo.abiString);
        addPairedString(EXCEPTION_INFO,
                PiiElider.sanitizeStacktrace(Log.getStackTraceString(javaException)));
        addPairedString(EARLY_JAVA_EXCEPTION, "true");
        addPairedString(PACKAGE,
                String.format("%s v%s (%s)", BuildInfo.getFirebaseAppId(), buildInfo.versionCode,
                        buildInfo.versionName));
        addPairedString(CUSTOM_THEMES, buildInfo.customThemes);
        addPairedString(RESOURCES_VERSION, buildInfo.resourcesVersion);

        AtomicReferenceArray<String> values = CrashKeys.getInstance().getValues();
        for (int i = 0; i < values.length(); i++) {
            String value = values.get(i);
            if (value != null) addPairedString(CrashKeys.getKey(i), value);
        }

        addString(mBoundary);
    }

    private void flushToFile() {
        if (mMinidumpFileStream != null) {
            try {
                mMinidumpFileStream.flush();
                mMinidumpFileStream.close();
            } catch (Throwable e) {
                mMinidumpFileStream = null;
                mMinidumpFile = null;
            }
        }
    }

    private static String getChannel() {
        if (ChromeVersionInfo.isCanaryBuild()) {
            return "canary";
        }
        if (ChromeVersionInfo.isDevBuild()) {
            return "dev";
        }
        if (ChromeVersionInfo.isBetaBuild()) {
            return "beta";
        }
        // An empty string indicates the stable channel.
        return "";
    }

    private static String detectCurrentProcessName() {
        try {
            int pid = android.os.Process.myPid();

            ActivityManager manager =
                    (ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                            Context.ACTIVITY_SERVICE);
            for (RunningAppProcessInfo processInfo : manager.getRunningAppProcesses()) {
                if (processInfo.pid == pid) {
                    return processInfo.processName;
                }
            }
            return null;
        } catch (SecurityException e) {
            return null;
        }
    }

    @VisibleForTesting
    public void uploadReport() {
        if (mMinidumpFile == null) return;
        LogcatExtractionRunnable logcatExtractionRunnable =
                new LogcatExtractionRunnable(mMinidumpFile);
        logcatExtractionRunnable.uploadMinidumpWithLogcat(true);
    }
}
