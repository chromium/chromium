// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.crash.NativeAndJavaSmartExceptionReporter;
import org.chromium.components.crash.PureJavaExceptionReporter;

import java.io.File;

/** A custom PureJavaExceptionReporter for Android Chrome's browser. */
@UsedByReflection("SplitCompatApplication.java")
@NullMarked
public class ChromePureJavaExceptionReporter extends PureJavaExceptionReporter {
    private static final String TAG = "ChromeCrashReporter";
    // LINT.IfChange(AndroidCrashProductName)
    private static final String CHROME_CRASH_PRODUCT_NAME = "Chrome_Android";
    private static final String CHROME_DESKTOP_CRASH_PRODUCT_NAME = "Chrome_Android_Desktop";
    // LINT.ThenChange(
    //   //chrome/app/chrome_crash_reporter_client.cc:AndroidCrashProductName,
    //   //components/minidump_uploader/rewrite_minidumps_as_mimes.cc:AndroidCrashProductName
    // )
    private static final String FILE_PREFIX = "chromium-browser-minidump-";

    @UsedByReflection("SplitCompatApplication.java")
    public ChromePureJavaExceptionReporter() {
        super(/* attachLogcat= */ true);
    }

    @Override
    protected File getCrashFilesDirectory() {
        return ContextUtils.getApplicationContext().getCacheDir();
    }

    @Override
    protected String getProductName() {
        // Differentiate Android Desktop crash reports by device type rather than UI affordance
        // (crbug.com/567605929).
        return DeviceInfo.isDesktop() // nocheck
                ? CHROME_DESKTOP_CRASH_PRODUCT_NAME
                : CHROME_CRASH_PRODUCT_NAME;
    }

    @Override
    protected void uploadMinidump(File minidump) {
        LogcatExtractionRunnable.uploadMinidump(minidump, true);
    }

    @Override
    protected String getMinidumpPrefix() {
        return FILE_PREFIX;
    }

    private static void reportPureJavaException(Throwable exception) {
        ChromePureJavaExceptionReporter reporter = new ChromePureJavaExceptionReporter();
        reporter.createAndUploadReport(exception);
    }

    /**
     * Asynchronously report and upload the stack trace as if it was a crash.
     *
     * @param msg The message to report.
     * @param isWarning Whether the message should be treated as a warning.
     */
    public static void reportJavaExceptionFromMsg(String msg, boolean isWarning) {
        if (isWarning) {
            msg = "This is not a crash. " + msg;
        }
        reportJavaException(new Throwable(msg));
    }

    /**
     * Asynchronously report and upload the stack trace as if it was a crash.
     *
     * @param exception The exception to report.
     */
    public static void reportJavaException(Throwable exception) {
        reportJavaException(exception, /* withLogWarning= */ true);
    }

    /**
     * Asynchronously report and upload the stack trace as if it was a crash. If |withLogging|,
     * include the exception message as log warning. This can be helpful e.g. to locate the logs
     * around when the exception is reported.
     *
     * @param exception The exception to report.
     * @param withLogWarning Whether to include the exception message as {@link Log#w}
     */
    public static void reportJavaException(Throwable exception, boolean withLogWarning) {
        if (withLogWarning) {
            String message = exception.getMessage();
            Log.w(TAG, message == null ? "" : message);
        }
        NativeAndJavaSmartExceptionReporter.postUploadReport(
                exception, ChromePureJavaExceptionReporter::reportPureJavaException);
    }
}
