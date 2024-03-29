// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.DeadSystemException;

import androidx.annotation.UiThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/**
 * This UncaughtExceptionHandler will create a breakpad minidump when there is an uncaught
 * exception.
 *
 * <p>The exception's stack trace will be added to the minidump's data. This allows java-only
 * crashes to be reported in the same way as other native crashes.
 */
@JNINamespace("base::android")
public class JavaExceptionReporter implements Thread.UncaughtExceptionHandler {
    private final Thread.UncaughtExceptionHandler mParent;
    private final boolean mCrashAfterReport;
    private boolean mHandlingException;

    private JavaExceptionReporter(
            Thread.UncaughtExceptionHandler parent, boolean crashAfterReport) {
        mParent = parent;
        mCrashAfterReport = crashAfterReport;
    }

    /**
     * Returns whether a given Throwable is meaningful and actionable and should be reported.
     *
     * <p>Removes the following exceptions:
     *
     * <ul>
     *   <li>DeadSystemException: The core Android system has died and is going through a restart.
     *       http://go/android-dev/reference/android/os/DeadSystemException
     * </ul>
     */
    public static boolean shouldReportThrowable(Throwable e) {
        return !(e instanceof DeadSystemException);
    }

    @Override
    public void uncaughtException(Thread t, Throwable e) {
        if (!mHandlingException && shouldReportThrowable(e)) {
            mHandlingException = true;
            JavaExceptionReporterJni.get()
                    .reportJavaException(
                            mCrashAfterReport,
                            // If we are dealing with a JNI uncaught exception, then `e` is just a
                            // wrapper around the true exception, annotated with the native stack
                            // trace. The native stack trace is redundant, since we're going to
                            // include it separately anyway. Remove it to make the report smaller,
                            // clearer and to prevent the true Java exception information from being
                            // truncated away.
                            e instanceof JniAndroid.UncaughtExceptionException ? e.getCause() : e);
        }
        if (mParent != null) {
            mParent.uncaughtException(t, e);
        }
    }

    /**
     * Report and upload the stack trace as if it was a crash. This is very expensive and should
     * be called rarely and only on the UI thread to avoid corrupting other crash uploads. Ideally
     * only called in idle handlers.
     *
     * @param stackTrace The stack trace to report.
     */
    @UiThread
    public static void reportStackTrace(String stackTrace) {
        assert ThreadUtils.runningOnUiThread();
        JavaExceptionReporterJni.get()
                .reportJavaStackTrace(PiiElider.sanitizeStacktrace(stackTrace));
    }

    /**
     * Report and upload the stack trace as if it was a crash. This is very expensive and should
     * be called rarely and only on the UI thread to avoid corrupting other crash uploads. Ideally
     * only called in idle handlers.
     *
     * @param exception The exception to report.
     */
    @UiThread
    public static void reportException(Throwable exception) {
        assert ThreadUtils.runningOnUiThread();
        JavaExceptionReporterJni.get().reportJavaException(false, exception);
    }

    @CalledByNative
    private static void installHandler(boolean crashAfterReport) {
        Thread.setDefaultUncaughtExceptionHandler(
                new JavaExceptionReporter(
                        Thread.getDefaultUncaughtExceptionHandler(), crashAfterReport));
    }

    @NativeMethods
    interface Natives {
        void reportJavaException(boolean crashAfterReport, Throwable e);

        void reportJavaStackTrace(@JniType("std::string") String stackTrace);
    }
}
