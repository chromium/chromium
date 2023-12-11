// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;

/** Provides Java-side code to back `jni_android` native logic. */
public final class JniAndroid {
    private JniAndroid() {}

    private static final String TAG = "JniAndroid";
    static boolean sSimulateOomInSanitizedStacktraceForTesting;

    /**
     * Returns a sanitized stacktrace (per {@link PiiElider#sanitizeStacktrace(String)}) for the
     * given throwable. Returns null if an OutOfMemoryError occurs.
     *
     * <p>Since this is running inside an uncaught exception handler, this method will make every
     * effort not to throw; instead, any failures will be surfaced through the returned string.
     */
    @CalledByNative
    private static String sanitizedStacktraceForUnhandledException(Throwable throwable) {
        if (sSimulateOomInSanitizedStacktraceForTesting) {
            return null;
        }
        try {
            return PiiElider.sanitizeStacktrace(Log.getStackTraceString(throwable));
        } catch (OutOfMemoryError oomError) {
            return null;
        } catch (Throwable stacktraceThrowable) {
            try {
                return "Error while getting stack trace: "
                        + Log.getStackTraceString(stacktraceThrowable);
            } catch (OutOfMemoryError oomError) {
                return null;
            }
        }
    }

    /**
     * Indicates that native code was faced with an uncaught Java exception.
     *
     * <p>{@code #getCause} returns the original uncaught exception.
     */
    public static class UncaughtExceptionException extends RuntimeException {
        public UncaughtExceptionException(String nativeStackTrace, Throwable uncaughtException) {
            super(
                    "Native stack trace:" + System.lineSeparator() + nativeStackTrace,
                    uncaughtException);
        }
    }

    /**
     * Called by the Chromium native JNI framework when faced with an uncaught Java exception while
     * executing a Java method from native code.
     *
     * <p>This method is expected to terminate the process (but is not guaranteed to).
     *
     * <p>The goal of this method is to provide an opportunity to terminate the process from the
     * Java side so that the crash looks like any other uncaught Java exception, and is handled
     * accordingly by system crash handlers. This ensures the Java stack trace will be collected, as
     * opposed to the native stack trace - the former is typically more useful as the true root
     * cause of the crash is Java code, not native code. See https://crbug.com/1426888 for more
     * discussion.
     *
     * <p>This method will make every effort not to throw to avoid re-entering the Chromium JNI
     * native exception handler. Errors will be sent to the system log instead.
     *
     * @param throwable The uncaught Java exception that was thrown by a Java method called via JNI.
     * @param nativeStackTrace The stack trace of the native code that called the Java method that
     *     threw.
     * @return null, unless the uncaught exception handler threw an exception other than
     *     OutOfMemoryError exception, in which case that exception is returned.
     */
    @CalledByNative
    private static Throwable handleException(Throwable throwable, String nativeStackTrace) {
        try {
            // Try to make sure the exception details at least make their way to the log even if the
            // rest of this method goes horribly wrong.
            Log.e(TAG, "Handling uncaught Java exception", throwable);

            // Wrap the original exception so that we can annotate it with native stack information,
            // with the goal of including as much information in the Java crash report as possible.
            // (The native caller might itself have been called from Java. We don't need to care
            // about that because the stack trace in `throwable` includes the *entire* Java stack of
            // the current thread, even if there are native calls in the middle.)
            var wrappedThrowable = new UncaughtExceptionException(nativeStackTrace, throwable);

            // The Chromium JNI framework does not support resuming execution after a Java method
            // called through JNI throws an exception - we have to terminate the process at some
            // point, otherwise undefined behavior may result. The goal here is to provide as much
            // useful information to the crash handler as we can.
            //
            // To that end, we try to call the global uncaught exception handler. Hopefully that
            // will eventually reach the default Android uncaught exception handler (possibly going
            // through JavaExceptionReporter first, if we set one up), which will terminate the
            // process. If for any reason that doesn't happen (e.g. the app set up a different
            // handler), then we just give up and return the new exception (if any) - the native
            // code we're returning to will terminate the process for us. (Note that, even then,
            // there is still a case where we might not terminate the process: if the uncaught
            // exception handler deliberately terminates the current thread but not the entire
            // process. This is very contrived though, and protecting against this would be
            // complicated, so we don't even try.)
            Thread.getDefaultUncaughtExceptionHandler()
                    .uncaughtException(Thread.currentThread(), wrappedThrowable);
            Log.e(TAG, "Global uncaught exception handler did not terminate the process.");
            return null;
        } catch (OutOfMemoryError e) {
            // Don't call Log.e() so as to not risk throwing again.
            return null;
        } catch (Throwable e) {
            // Log the new crash rather than the original crash, since if there is a bug in our
            // crash handling logic, we need to know about it. If there is a crash in a webview
            // app's crash handling logic, then we can rely on other apps to upload the underlying
            // exception.
            Log.e(TAG, "Exception in uncaught exception handler.", e);
            return e;
        }
    }
}
