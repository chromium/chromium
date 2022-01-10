// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

/**
 * This class has been moved to //components/crash/android.
 */
// TODO(crbug.com/1081916): Remove this after migrating downstream deps to use
// ChromePureJavaExceptionReporter.
public class PureJavaExceptionReporter {
    /**
     * Report and upload the device info and stack trace as if it was a crash. Runs synchronously
     * and results in I/O on the main thread.
     *
     * @param javaException The exception to report.
     */
    public static void reportJavaException(Throwable javaException) {
        ChromePureJavaExceptionReporter.reportJavaException(javaException);
    }

    /**
     * Posts a task to report and upload the device info and stack trace as if it was a crash.
     *
     * @param javaException The exception to report.
     */
    public static void postReportJavaException(Throwable javaException) {
        ChromePureJavaExceptionReporter.postReportJavaException(javaException);
    }
}
