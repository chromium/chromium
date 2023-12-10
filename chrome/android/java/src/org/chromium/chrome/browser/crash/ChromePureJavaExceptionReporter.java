// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.crash.NativeAndJavaSmartExceptionReporter;
import org.chromium.components.crash.PureJavaExceptionReporter;

import java.io.File;

/** A custom PureJavaExceptionReporter for Android Chrome's browser. */
@UsedByReflection("SplitCompatApplication.java")
public class ChromePureJavaExceptionReporter extends PureJavaExceptionReporter {
    private static final String CHROME_CRASH_PRODUCT_NAME = "Chrome_Android";
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

    private static void reportPureJavaException(Throwable exception) {
        ChromePureJavaExceptionReporter reporter = new ChromePureJavaExceptionReporter();
        reporter.createAndUploadReport(exception);
    }

    /**
     * Asynchronously report and upload the stack trace as if it was a crash.
     *
     * @param exception The exception to report.
     */
    public static void reportJavaException(Throwable exception) {
        NativeAndJavaSmartExceptionReporter.postUploadReport(
                exception, ChromePureJavaExceptionReporter::reportPureJavaException);
    }
}
