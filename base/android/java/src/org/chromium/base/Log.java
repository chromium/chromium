// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.AlwaysInline;

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
     * When BuildConfig.ENABLE_DEBUG_LOGS=true, returns true. Otherwise, forwards to {@link
     * android.util.Log#isLoggable(String, int)} (which returns false for levels < INFO, unless
     * configured otherwise by R8's -maximumremovedandroidloglevel).
     *
     * <p>https://stackoverflow.com/questions/7948204/does-log-isloggable-returns-wrong-values
     */
    @AlwaysInline
    public static boolean isLoggable(String tag, int level) {
        return BuildConfig.ENABLE_DEBUG_LOGS || android.util.Log.isLoggable(tag, level);
    }

    /**
     * Sends a {@link android.util.Log#VERBOSE} log message.
     *
     * @param tag Used to identify the source of a log message. Might be modified in the output (see
     *     {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *     string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *     one is a {@link Throwable}, its trace will be printed.
     */
    public static void v(String tag, String messageTemplate, Object... args) {
        if (!isLoggable(tag, VERBOSE)) return;

        Throwable tr = getThrowableToLog(args);
        String message = formatLog(messageTemplate, tr, args);
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
     * @param tag Used to identify the source of a log message. Might be modified in the output (see
     *     {@link #normalizeTag(String)})
     * @param messageTemplate The message you would like logged. It is to be specified as a format
     *     string.
     * @param args Arguments referenced by the format specifiers in the format string. If the last
     *     one is a {@link Throwable}, its trace will be printed.
     */
    public static void d(String tag, String messageTemplate, Object... args) {
        if (!isLoggable(tag, DEBUG)) return;

        Throwable tr = getThrowableToLog(args);
        String message = formatLog(messageTemplate, tr, args);
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

    // Overloads that will optimize better:
    // * No need to call getThrowableToLog()
    // * normalizeTag() will be evaluated to const-string "cr_..."
    // * String.format() will be converted into StringBuilder when possible
    //   * Which also removes auto-boxing of primitives.
    @AlwaysInline
    public static void i(String tag, String message) {
        android.util.Log.i(normalizeTag(tag), message);
    }
    @AlwaysInline
    public static void i(String tag, String message, Throwable t) {
        android.util.Log.i(normalizeTag(tag), message, t);
    }

    @AlwaysInline
    public static void i(String tag, String messageTemplate, Object param1) {
        tag = normalizeTag(tag);
        android.util.Log.i(tag, String.format(Locale.US, messageTemplate, param1));
    }

    @AlwaysInline
    public static void i(String tag, String messageTemplate, Object param1, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.i(tag, String.format(Locale.US, messageTemplate, param1), t);
    }

    @AlwaysInline
    public static void i(String tag, String messageTemplate, Object param1, Object param2) {
        tag = normalizeTag(tag);
        android.util.Log.i(tag, String.format(Locale.US, messageTemplate, param1, param2));
    }

    @AlwaysInline
    public static void i(
            String tag, String messageTemplate, Object param1, Object param2, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.i(tag, String.format(Locale.US, messageTemplate, param1, param2), t);
    }

    @AlwaysInline
    public static void i(
            String tag, String messageTemplate, Object param1, Object param2, Object param3) {
        tag = normalizeTag(tag);
        android.util.Log.i(tag, String.format(Locale.US, messageTemplate, param1, param2, param3));
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3), t);
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4));
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4), t);
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5));
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5),
                t);
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag,
                String.format(
                        Locale.US,
                        messageTemplate,
                        param1,
                        param2,
                        param3,
                        param4,
                        param5,
                        param6));
    }

    @AlwaysInline
    public static void i(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.i(
                tag,
                String.format(
                        Locale.US, messageTemplate, param1, param2, param3, param4, param5, param6),
                t);
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

    // Overloads that will optimize better:
    // * No need to call getThrowableToLog()
    // * normalizeTag() will be evaluated to const-string "cr_..."
    // * String.format() will be converted into StringBuilder when possible
    //   * Which also removes auto-boxing of primitives.
    @AlwaysInline
    public static void w(String tag, String message) {
        android.util.Log.w(normalizeTag(tag), message);
    }
    @AlwaysInline
    public static void w(String tag, String message, Throwable t) {
        android.util.Log.w(normalizeTag(tag), message, t);
    }

    @AlwaysInline
    public static void w(String tag, String messageTemplate, Object param1) {
        tag = normalizeTag(tag);
        android.util.Log.w(tag, String.format(Locale.US, messageTemplate, param1));
    }

    @AlwaysInline
    public static void w(String tag, String messageTemplate, Object param1, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.w(tag, String.format(Locale.US, messageTemplate, param1), t);
    }

    @AlwaysInline
    public static void w(String tag, String messageTemplate, Object param1, Object param2) {
        tag = normalizeTag(tag);
        android.util.Log.w(tag, String.format(Locale.US, messageTemplate, param1, param2));
    }

    @AlwaysInline
    public static void w(
            String tag, String messageTemplate, Object param1, Object param2, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.w(tag, String.format(Locale.US, messageTemplate, param1, param2), t);
    }

    @AlwaysInline
    public static void w(
            String tag, String messageTemplate, Object param1, Object param2, Object param3) {
        tag = normalizeTag(tag);
        android.util.Log.w(tag, String.format(Locale.US, messageTemplate, param1, param2, param3));
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3), t);
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4));
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4), t);
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5));
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5),
                t);
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag,
                String.format(
                        Locale.US,
                        messageTemplate,
                        param1,
                        param2,
                        param3,
                        param4,
                        param5,
                        param6));
    }

    @AlwaysInline
    public static void w(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.w(
                tag,
                String.format(
                        Locale.US, messageTemplate, param1, param2, param3, param4, param5, param6),
                t);
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

    // Overloads that will optimize better:
    // * No need to call getThrowableToLog()
    // * normalizeTag() will be evaluated to const-string "cr_..."
    // * String.format() will be converted into StringBuilder when possible
    //   * Which also removes auto-boxing of primitives.
    @AlwaysInline
    public static void e(String tag, String message) {
        android.util.Log.e(normalizeTag(tag), message);
    }
    @AlwaysInline
    public static void e(String tag, String message, Throwable t) {
        android.util.Log.e(normalizeTag(tag), message, t);
    }

    @AlwaysInline
    public static void e(String tag, String messageTemplate, Object param1) {
        tag = normalizeTag(tag);
        android.util.Log.e(tag, String.format(Locale.US, messageTemplate, param1));
    }

    @AlwaysInline
    public static void e(String tag, String messageTemplate, Object param1, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.e(tag, String.format(Locale.US, messageTemplate, param1), t);
    }

    @AlwaysInline
    public static void e(String tag, String messageTemplate, Object param1, Object param2) {
        tag = normalizeTag(tag);
        android.util.Log.e(tag, String.format(Locale.US, messageTemplate, param1, param2));
    }

    @AlwaysInline
    public static void e(
            String tag, String messageTemplate, Object param1, Object param2, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.e(tag, String.format(Locale.US, messageTemplate, param1, param2), t);
    }

    @AlwaysInline
    public static void e(
            String tag, String messageTemplate, Object param1, Object param2, Object param3) {
        tag = normalizeTag(tag);
        android.util.Log.e(tag, String.format(Locale.US, messageTemplate, param1, param2, param3));
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3), t);
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4));
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4), t);
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5));
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5),
                t);
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag,
                String.format(
                        Locale.US,
                        messageTemplate,
                        param1,
                        param2,
                        param3,
                        param4,
                        param5,
                        param6));
    }

    @AlwaysInline
    public static void e(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.e(
                tag,
                String.format(
                        Locale.US, messageTemplate, param1, param2, param3, param4, param5, param6),
                t);
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

    // Overloads that will optimize better:
    // * No need to call getThrowableToLog()
    // * normalizeTag() will be evaluated to const-string "cr_..."
    // * String.format() will be converted into StringBuilder when possible
    //   * Which also removes auto-boxing of primitives.
    @AlwaysInline
    public static void wtf(String tag, String message) {
        android.util.Log.wtf(normalizeTag(tag), message);
    }

    @AlwaysInline
    public static void wtf(String tag, String message, Throwable t) {
        android.util.Log.wtf(normalizeTag(tag), message, t);
    }

    @AlwaysInline
    public static void wtf(String tag, String messageTemplate, Object param1) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(tag, String.format(Locale.US, messageTemplate, param1));
    }

    @AlwaysInline
    public static void wtf(String tag, String messageTemplate, Object param1, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(tag, String.format(Locale.US, messageTemplate, param1), t);
    }

    @AlwaysInline
    public static void wtf(String tag, String messageTemplate, Object param1, Object param2) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(tag, String.format(Locale.US, messageTemplate, param1, param2));
    }

    @AlwaysInline
    public static void wtf(
            String tag, String messageTemplate, Object param1, Object param2, Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(tag, String.format(Locale.US, messageTemplate, param1, param2), t);
    }

    @AlwaysInline
    public static void wtf(
            String tag, String messageTemplate, Object param1, Object param2, Object param3) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3));
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3), t);
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4));
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag, String.format(Locale.US, messageTemplate, param1, param2, param3, param4), t);
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5));
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag,
                String.format(Locale.US, messageTemplate, param1, param2, param3, param4, param5),
                t);
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag,
                String.format(
                        Locale.US,
                        messageTemplate,
                        param1,
                        param2,
                        param3,
                        param4,
                        param5,
                        param6));
    }

    @AlwaysInline
    public static void wtf(
            String tag,
            String messageTemplate,
            Object param1,
            Object param2,
            Object param3,
            Object param4,
            Object param5,
            Object param6,
            Throwable t) {
        tag = normalizeTag(tag);
        android.util.Log.wtf(
                tag,
                String.format(
                        Locale.US, messageTemplate, param1, param2, param3, param4, param5, param6),
                t);
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
}
