// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.MainDex;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.crash.PureJavaExceptionReporter;

import java.io.File;

/**
 * A custom PureJavaExceptionReporter for Android Chrome's browser.
 */
@MainDex
@UsedByReflection("SplitCompatApplication.java")
public class ChromePureJavaExceptionReporter extends PureJavaExceptionReporter {
    private static final String CHROME_CRASH_PRODUCT_NAME = "Chrome_Android";
    private static final String FILE_PREFIX = "chromium-browser-minidump-";

    @UsedByReflection("SplitCompatApplication.java")
    public ChromePureJavaExceptionReporter() {
        super(/*attachLogcat=*/true);
    }

    @Override
    protected File getCrashFilesDirectory() {
        return ContextUtils.getApplicationContext().getCacheDir();
    }

    @Override
    protected String getProductName() {
        return CHROME_CRASH_PRODUCT_NAME;
    }

    @Override
    protected void uploadMinidump(File minidump) {
        LogcatExtractionRunnable.uploadMinidump(minidump, true);
    }

    @Override
    protected String getMinidumpPrefix() {
        return FILE_PREFIX;
    }

    /**
     * Report and upload the device info and stack trace as if it was a crash. Runs synchronously
     * and results in I/O on the main thread.
     *
     * @param javaException The exception to report.
     */
    public static void reportJavaException(Throwable javaException) {
        ChromePureJavaExceptionReporter reporter = new ChromePureJavaExceptionReporter();
        reporter.createAndUploadReport(javaException);
    }

    /**
     * Posts a task to report and upload the device info and stack trace as if it was a crash.
     *
     * @param javaException The exception to report.
     */
    public static void postReportJavaException(Throwable javaException) {
        ChromePureJavaExceptionReporter reporter = new ChromePureJavaExceptionReporter();
        reporter.postCreateAndUploadReport(javaException);
    }
}
