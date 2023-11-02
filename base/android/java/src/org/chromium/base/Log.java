// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.AlwaysInline;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.DoNotInline;

import java.util.Locale;

/**
 * Utility class for Logging.
 *
 * <p>
 * Defines logging access points for each feature. They format and forward the logs to
 * {@link android.util.Log}, allowing to standardize the output, to make it easy to identify
 * the origin of logs, and enable or disable logging in different parts of the code.
 * </p>
 * <p>
 * Usage documentation: {@code //docs/android_logging.md}.
 * </p>
 */
public class Log {
    /** Convenience property, same as {@link android.util.Log#ASSERT}. */
    public static final int ASSERT = android.util.Log.ASSERT;

    /** Convenience property, same as {@link android.util.Log#DEBUG}. */
    public static final int DEBUG = android.util.Log.DEBUG;

    /** Convenience property, same as {@link android.util.Log#ERROR}. */
    public static final int ERROR = android.util.Log.ERROR;

    /** Convenience property, same as {@link android.util.Log#INFO}. */
    public static final int INFO = android.util.Log.INFO;

    /** Convenience property, same as {@link android.util.Log#VERBOSE}. */
    public static final int VERBOSE = android.util.Log.VERBOSE;

    /** Convenience property, same as {@link android.util.Log#WARN}. */
    public static final int WARN = android.util.Log.WARN;

    private Log() {
        // Static only access
    }

    /** Returns a formatted log message, using the supplied format and arguments.*/
    private static String formatLog(String messageTemplate, Throwable tr, Object... params) {
        if ((params != null) && ((tr == null && params.length > 0) || params.length > 1)) {
            messageTemplate = String.format(Locale.US, messageTemplate, params);
        }

        return messageTemplate;
    }

    /**
     * Returns a normalized tag that will be in the form: "cr_foo". This function is called by the
     * various Log overrides. If using {@link #isLoggable(String, int)}, you might want to call it
     * to get the tag that will actually be used.
     */
    @AlwaysInline
    public static String normalizeTag(String tag) {
        // @AlwaysInline makes sense because this method is almost always called with a string
        // literal as a parameter, so inlining causes the .concat() to happen at build-time.
        return "cr_" + tag;
    }

    /**
     * Returns a formatted log message, using the supplied format and arguments.
     * The message will be prepended with the filename and line number of the call.
     */
    private static String formatLogWithStack(
            String messageTemplate, Throwable tr, Object... params) {
        return "[" + getCallOrigin() + "] " + formatLog(messageTemplate, tr, params);
    }

    private static boolean isDebug() {
        // Proguard sets value to false in release builds.
        return true;
    }

    /**
     * In debug: Forwards to {@link android.util.Log#isLoggable(String, int)}, but always
     * In release: Always returns false (via proguard rule).
     */
    public static boolean isLoggable(String tag, int level) {
        // Early return helps optimizer eliminate calls to isLoggable().
        if (!isDebug() && level <= INFO) {
            return false;
        }
        return android.util.Log.isLoggable(tag, level);
    }

    /**
     * Sends a {@link android.util.Log#VERBOSE} log message.
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output
     *            (see {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    @CheckDiscard("crbug.com/1231625")
    public static void v(String tag, String messageTemplate, Object... args) {
        if (!isDebug()) return;

        Throwable tr = getThrowableToLog(args);
        String message = formatLogWithStack(messageTemplate, tr, args);
        tag = normalizeTag(tag);
        if (tr != null) {
            android.util.Log.v(tag, message, tr);
        } else {
            android.util.Log.v(tag, message);
        }
    }

    /**
     * Sends a {@link android.util.Log#DEBUG} log message.
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output
     *            (see {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    @CheckDiscard("crbug.com/1231625")
    public static void d(String tag, String messageTemplate, Object... args) {
        if (!isDebug()) return;

        Throwable tr = getThrowableToLog(args);
        String message = formatLogWithStack(messageTemplate, tr, args);
        tag = normalizeTag(tag);
        if (tr != null) {
            android.util.Log.d(tag, message, tr);
        } else {
            android.util.Log.d(tag, message);
        }
    }

    /**
     * Sends an {@link android.util.Log#INFO} log message.
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output
     *            (see {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void i(String tag, String messageTemplate, Object... args) {
        Throwable tr = getThrowableToLog(args);
        String message = formatLog(messageTemplate, tr, args);
        tag = normalizeTag(tag);
        if (tr != null) {
            android.util.Log.i(tag, message, tr);
        } else {
            android.util.Log.i(tag, message);
        }
    }

    // Overloads to avoid varargs overhead.
    @AlwaysInline
    public static void i(String tag, String message) {
        android.util.Log.i(normalizeTag(tag), message);
    }
    @AlwaysInline
    public static void i(String tag, String message, Throwable t) {
        android.util.Log.i(normalizeTag(tag), message, t);
    }
    @DoNotInline
    public static void i(String tag, String messageTemplate, Object o) {
        i(tag, messageTemplate, new Object[] {o});
    }
    @DoNotInline
    public static void i(String tag, String messageTemplate, Object o1, Object o2) {
        i(tag, messageTemplate, new Object[] {o1, o2});
    }

    /**
     * Sends a {@link android.util.Log#WARN} log message.
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output
     *            (see {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void w(String tag, String messageTemplate, Object... args) {
        Throwable tr = getThrowableToLog(args);
        String message = formatLog(messageTemplate, tr, args);
        tag = normalizeTag(tag);
        if (tr != null) {
            android.util.Log.w(tag, message, tr);
        } else {
            android.util.Log.w(tag, message);
        }
    }

    // Overloads to avoid varargs overhead.
    @AlwaysInline
    public static void w(String tag, String message) {
        android.util.Log.w(normalizeTag(tag), message);
    }
    @AlwaysInline
    public static void w(String tag, String message, Throwable t) {
        android.util.Log.w(normalizeTag(tag), message, t);
    }
    @DoNotInline
    public static void w(String tag, String messageTemplate, Object o) {
        w(tag, messageTemplate, new Object[] {o});
    }
    @DoNotInline
    public static void w(String tag, String messageTemplate, Object o1, Object o2) {
        w(tag, messageTemplate, new Object[] {o1, o2});
    }

    /**
     * Sends an {@link android.util.Log#ERROR} log message.
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output
     *            (see {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void e(String tag, String messageTemplate, Object... args) {
        Throwable tr = getThrowableToLog(args);
        String message = formatLog(messageTemplate, tr, args);
        tag = normalizeTag(tag);
        if (tr != null) {
            android.util.Log.e(tag, message, tr);
        } else {
            android.util.Log.e(tag, message);
        }
    }

    // Overloads to avoid varargs overhead.
    @AlwaysInline
    public static void e(String tag, String message) {
        android.util.Log.e(normalizeTag(tag), message);
    }
    @AlwaysInline
    public static void e(String tag, String message, Throwable t) {
        android.util.Log.e(normalizeTag(tag), message, t);
    }
    @DoNotInline
    public static void e(String tag, String messageTemplate, Object o) {
        e(tag, messageTemplate, new Object[] {o});
    }
    @DoNotInline
    public static void e(String tag, String messageTemplate, Object o1, Object o2) {
        e(tag, messageTemplate, new Object[] {o1, o2});
    }

    /**
     * What a Terrible Failure: Used for conditions that should never happen, and logged at
     * the {@link android.util.Log#ASSERT} level. Depending on the configuration, it might
     * terminate the process.
     *
     * @see android.util.Log#wtf(String, String, Throwable)
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output
     *            (see {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *                        string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *             one is a {@link Throwable}, its trace will be printed.
     */
    public static void wtf(String tag, String messageTemplate, Object... args) {
        Throwable tr = getThrowableToLog(args);
        String message = formatLog(messageTemplate, tr, args);
        tag = normalizeTag(tag);
        if (tr != null) {
            android.util.Log.wtf(tag, message, tr);
        } else {
            android.util.Log.wtf(tag, message);
        }
    }

    /** Handy function to get a loggable stack trace from a Throwable. */
    public static String getStackTraceString(Throwable tr) {
        return android.util.Log.getStackTraceString(tr);
    }

    private static Throwable getThrowableToLog(Object[] args) {
        if (args == null || args.length == 0) return null;

        Object lastArg = args[args.length - 1];

        if (!(lastArg instanceof Throwable)) return null;
        return (Throwable) lastArg;
    }

    /** Returns a string form of the origin of the log call, to be used as secondary tag.*/
    @CheckDiscard("crbug.com/1231625")
    private static String getCallOrigin() {
        StackTraceElement[] st = Thread.currentThread().getStackTrace();

        // The call stack should look like:
        //   n [a variable number of calls depending on the vm used]
        //  +0 getCallOrigin()
        //  +1 formatLogWithStack()
        //  +2 privateLogFunction: verbose or debug
        //  +3 caller

        int callerStackIndex;
        String logClassName = Log.class.getName();
        for (callerStackIndex = 0; callerStackIndex < st.length; callerStackIndex++) {
            if (st[callerStackIndex].getClassName().equals(logClassName)) {
                callerStackIndex += 3;
                break;
            }
        }

        return st[callerStackIndex].getFileName() + ":" + st[callerStackIndex].getLineNumber();
    }
}
