// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Process;
import android.os.StrictMode;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.concurrent.GuardedBy;

/**
 * Support for early tracing, before the native library is loaded.
 *
 * Note that arguments are not currently supported for early events, but could
 * be added in the future.
 *
 * Events recorded here are buffered in Java until the native library is available, at which point
 * they are flushed to the native side and regular java tracing (TraceEvent) takes over.
 *
 * Locking: This class is threadsafe. It is enabled when general tracing is, and then disabled when
 *          tracing is enabled from the native side. At this point, buffered events are flushed to
 *          the native side and then early tracing is permanently disabled after dumping the events.
 *
 * Like the TraceEvent, the event name of the trace events must be a string literal or a |static
 * final String| class member. Otherwise NoDynamicStringsInTraceEventCheck error will be thrown.
 */
@JNINamespace("base::android")
public class EarlyTraceEvent {
    /** Single trace event. */
    @VisibleForTesting
    static final class Event {
        final boolean mIsStart;
        final boolean mIsToplevel;
        final String mName;
        final int mThreadId;
        final long mTimeNanos;
        final long mThreadTimeMillis;

        Event(String name, boolean isStart, boolean isToplevel) {
            mIsStart = isStart;
            mIsToplevel = isToplevel;
            mName = name;
            mThreadId = Process.myTid();
            mTimeNanos = System.nanoTime(); // Same timebase as TimeTicks::Now().
            mThreadTimeMillis = SystemClock.currentThreadTimeMillis();
        }
    }

    @VisibleForTesting
    static final class AsyncEvent {
        final boolean mIsStart;
        final String mName;
        final long mId;
        final long mTimeNanos;

        AsyncEvent(String name, long id, boolean isStart) {
            mName = name;
            mId = id;
            mIsStart = isStart;
            mTimeNanos = System.nanoTime(); // Same timebase as TimeTicks::Now().
        }
    }

    @VisibleForTesting
    static final class ActivityStartupEvent {
        final long mId;
        final long mTimeMs;

        ActivityStartupEvent(long id, long timeMs) {
            mId = id;
            mTimeMs = timeMs;
        }
    }

    @VisibleForTesting
    static final class ActivityLaunchCauseEvent {
        final long mId;
        final long mTimeMs;
        final int mLaunchCause;

        ActivityLaunchCauseEvent(long id, int launchCause) {
            mId = id;
            mTimeMs = SystemClock.uptimeMillis();
            mLaunchCause = launchCause;
        }
    }

    // State transitions are:
    // - enable(): DISABLED -> ENABLED
    // - disable(): ENABLED -> FINISHED
    @VisibleForTesting static final int STATE_DISABLED = 0;
    @VisibleForTesting static final int STATE_ENABLED = 1;
    @VisibleForTesting static final int STATE_FINISHED = 2;
    @VisibleForTesting static volatile int sState = STATE_DISABLED;

    // In child processes the CommandLine is not available immediately, so early tracing is enabled
    // unconditionally in Chrome. This flag allows not to enable early tracing twice in this case.
    private static volatile boolean sEnabledInChildProcessBeforeCommandLine;

    private static final String BACKGROUND_STARTUP_TRACING_ENABLED_KEY = "bg_startup_tracing";
    private static boolean sCachedBackgroundStartupTracingFlag;

    // Early tracing can be enabled on browser start if the browser finds this file present. Must be
    // kept in sync with the native kAndroidTraceConfigFile.
    private static final String TRACE_CONFIG_FILENAME = "/data/local/chrome-trace-config.json";

    // Early tracing can be enabled on browser start if the browser finds this command line switch.
    // Must be kept in sync with switches::kTraceStartup.
    private static final String TRACE_STARTUP_SWITCH = "trace-startup";

    // Added to child process switches if tracing is enabled when the process is getting created.
    // The flag is checked early in child process lifetime to have a solid guarantee that the early
    // java tracing is not enabled forever. Native flags cannot be used for this purpose because the
    // native library is not loaded at the moment. Cannot set --trace-startup for the child to avoid
    // overriding the list of categories it may load from the config later. Also --trace-startup
    // depends on other flags that early tracing should not know about. Public for use in
    // ChildProcessLauncherHelperImpl.
    public static final String TRACE_EARLY_JAVA_IN_CHILD_SWITCH = "trace-early-java-in-child";

    // Protects the fields below.
    @VisibleForTesting static final Object sLock = new Object();

    // Not final because in many configurations these objects are not used.
    @GuardedBy("sLock")
    @VisibleForTesting
    static List<Event> sEvents;

    @GuardedBy("sLock")
    @VisibleForTesting
    static List<AsyncEvent> sAsyncEvents;

    @GuardedBy("sLock")
    @VisibleForTesting
    static final List<ActivityStartupEvent> sActivityStartupEvents =
            new ArrayList<ActivityStartupEvent>();

    @GuardedBy("sLock")
    @VisibleForTesting
    static final List<ActivityLaunchCauseEvent> sActivityLaunchCauseEvents =
            new ArrayList<ActivityLaunchCauseEvent>();

    /** @see TraceEvent#maybeEnableEarlyTracing(boolean) */
    static void maybeEnableInBrowserProcess() {
        ThreadUtils.assertOnUiThread();
        assert !sEnabledInChildProcessBeforeCommandLine
                : "Should not have been initialized in a child process";
        if (sState != STATE_DISABLED) return;
        boolean shouldEnable = false;
        // Checking for the trace config filename touches the disk.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            if (CommandLine.getInstance().hasSwitch(TRACE_STARTUP_SWITCH)) {
                shouldEnable = true;
            } else {
                try {
                    shouldEnable = new File(TRACE_CONFIG_FILENAME).exists();
                } catch (SecurityException e) {
                    // Access denied, not enabled.
                }
            }
            if (ContextUtils.getAppSharedPreferences()
                    .getBoolean(BACKGROUND_STARTUP_TRACING_ENABLED_KEY, false)) {
                if (shouldEnable) {
                    // If user has enabled tracing, then force disable background tracing for this
                    // session.
                    setBackgroundStartupTracingFlag(false);
                    sCachedBackgroundStartupTracingFlag = false;
                } else {
                    sCachedBackgroundStartupTracingFlag = true;
                    shouldEnable = true;
                }
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        if (shouldEnable) enable();
    }

    /** Enables early tracing in child processes before CommandLine arrives there. */
    public static void earlyEnableInChildWithoutCommandLine() {
        sEnabledInChildProcessBeforeCommandLine = true;
        assert sState == STATE_DISABLED;
        enable();
    }

    /**
     * Based on a command line switch from the process launcher, enables or resets early tracing.
     * Should be called only in child processes and as soon as possible after the CommandLine is
     * initialized.
     */
    public static void onCommandLineAvailableInChildProcess() {
        // Ignore early Java tracing in WebView and other startup configurations that did not start
        // collecting events before the command line was available.
        if (!sEnabledInChildProcessBeforeCommandLine) return;
        synchronized (sLock) {
            // Remove early trace events if the child process launcher did not ask for early
            // tracing.
            if (!CommandLine.getInstance().hasSwitch(TRACE_EARLY_JAVA_IN_CHILD_SWITCH)) {
                reset();
                return;
            }
            // Otherwise continue with tracing enabled.
            if (sState == STATE_DISABLED) enable();
        }
    }

    static void enable() {
        synchronized (sLock) {
            if (sState != STATE_DISABLED) return;
            sEvents = new ArrayList<Event>();
            sAsyncEvents = new ArrayList<AsyncEvent>();
            sState = STATE_ENABLED;
        }
    }

    /**
     * Disables Early tracing and flushes buffered events to the native side.
     *
     * Once this is called, no new event will be registered.
     */
    static void disable() {
        synchronized (sLock) {
            if (!enabled()) return;

            if (!sEvents.isEmpty()) {
                dumpEvents(sEvents);
                sEvents.clear();
            }
            if (!sAsyncEvents.isEmpty()) {
                dumpAsyncEvents(sAsyncEvents);
                sAsyncEvents.clear();
            }

            sState = STATE_FINISHED;
            sEvents = null;
            sAsyncEvents = null;
        }
    }

    /** Stops early tracing without flushing the buffered events. */
    @VisibleForTesting
    static void reset() {
        synchronized (sLock) {
            sState = STATE_DISABLED;
            sEvents = null;
            sAsyncEvents = null;
        }
    }

    static boolean enabled() {
        return sState == STATE_ENABLED;
    }

    /** Sets the background startup tracing enabled in app preferences for next startup. */
    @CalledByNative
    static void setBackgroundStartupTracingFlag(boolean enabled) {
        // Setting preferences might cause a disk write
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putBoolean(BACKGROUND_STARTUP_TRACING_ENABLED_KEY, enabled)
                    .apply();
        }
    }

    /**
     * Returns true if the background startup tracing flag is set.
     *
     * This does not return the correct value if called before maybeEnable() was called. But that is
     * called really early in startup.
     */
    @CalledByNative
    public static boolean getBackgroundStartupTracingFlag() {
        return sCachedBackgroundStartupTracingFlag;
    }

    /** @see TraceEvent#begin */
    public static void begin(String name, boolean isToplevel) {
        // begin() and end() are going to be called once per TraceEvent, this avoids entering a
        // synchronized block at each and every call.
        if (!enabled()) return;
        Event event = new Event(name, /* isStart= */ true, isToplevel);
        synchronized (sLock) {
            if (!enabled()) return;
            sEvents.add(event);
        }
    }

    /** @see TraceEvent#end */
    public static void end(String name, boolean isToplevel) {
        if (!enabled()) return;
        Event event = new Event(name, /* isStart= */ false, isToplevel);
        synchronized (sLock) {
            if (!enabled()) return;
            sEvents.add(event);
        }
    }

    /** @see TraceEvent#startAsync */
    public static void startAsync(String name, long id) {
        if (!enabled()) return;
        AsyncEvent event = new AsyncEvent(name, id, /* isStart= */ true);
        synchronized (sLock) {
            if (!enabled()) return;
            sAsyncEvents.add(event);
        }
    }

    /** @see TraceEvent#finishAsync */
    public static void finishAsync(String name, long id) {
        if (!enabled()) return;
        AsyncEvent event = new AsyncEvent(name, id, /* isStart= */ false);
        synchronized (sLock) {
            if (!enabled()) return;
            sAsyncEvents.add(event);
        }
    }

    /**
     * @see TraceEvent#startupActivityStart
     */
    public static void startupActivityStart(long activityId, long startTimeMs) {
        ActivityStartupEvent event = new ActivityStartupEvent(activityId, startTimeMs);
        synchronized (sLock) {
            sActivityStartupEvents.add(event);
        }
    }

    /**
     * @see TraceEvent#startupLaunchCause
     */
    public static void startupLaunchCause(long activityId, int launchCause) {
        ActivityLaunchCauseEvent event = new ActivityLaunchCauseEvent(activityId, launchCause);
        synchronized (sLock) {
            sActivityLaunchCauseEvents.add(event);
        }
    }

    static List<Event> getMatchingCompletedEventsForTesting(String eventName) {
        synchronized (sLock) {
            List<Event> matchingEvents = new ArrayList<Event>();
            for (Event evt : EarlyTraceEvent.sEvents) {
                if (evt.mName.equals(eventName)) {
                    matchingEvents.add(evt);
                }
            }
            return matchingEvents;
        }
    }

    private static void dumpEvents(List<Event> events) {
        for (Event e : events) {
            if (e.mIsStart) {
                if (e.mIsToplevel) {
                    EarlyTraceEventJni.get()
                            .recordEarlyToplevelBeginEvent(
                                    e.mName, e.mTimeNanos, e.mThreadId, e.mThreadTimeMillis);
                } else {
                    EarlyTraceEventJni.get()
                            .recordEarlyBeginEvent(
                                    e.mName, e.mTimeNanos, e.mThreadId, e.mThreadTimeMillis);
                }
            } else {
                if (e.mIsToplevel) {
                    EarlyTraceEventJni.get()
                            .recordEarlyToplevelEndEvent(
                                    e.mName, e.mTimeNanos, e.mThreadId, e.mThreadTimeMillis);
                } else {
                    EarlyTraceEventJni.get()
                            .recordEarlyEndEvent(
                                    e.mName, e.mTimeNanos, e.mThreadId, e.mThreadTimeMillis);
                }
            }
        }
    }

    private static void dumpAsyncEvents(List<AsyncEvent> events) {
        for (AsyncEvent e : events) {
            if (e.mIsStart) {
                EarlyTraceEventJni.get().recordEarlyAsyncBeginEvent(e.mName, e.mId, e.mTimeNanos);
            } else {
                EarlyTraceEventJni.get().recordEarlyAsyncEndEvent(e.mId, e.mTimeNanos);
            }
        }
    }

    /** Can only be called if the TraceEventJni has been enabled. */
    public static void dumpActivityStartupEvents() {
        synchronized (sLock) {
            if (!sActivityStartupEvents.isEmpty()) {
                for (ActivityStartupEvent e : sActivityStartupEvents) {
                    TraceEventJni.get().startupActivityStart(e.mId, e.mTimeMs);
                }
                sActivityStartupEvents.clear();
            }
            if (!sActivityLaunchCauseEvents.isEmpty()) {
                for (ActivityLaunchCauseEvent e : sActivityLaunchCauseEvents) {
                    TraceEventJni.get().startupLaunchCause(e.mId, e.mTimeMs, e.mLaunchCause);
                }
                sActivityLaunchCauseEvents.clear();
            }
        }
    }

    @NativeMethods
    interface Natives {
        void recordEarlyBeginEvent(String name, long timeNanos, int threadId, long threadMillis);

        void recordEarlyEndEvent(String name, long timeNanos, int threadId, long threadMillis);

        void recordEarlyToplevelBeginEvent(
                String name, long timeNanos, int threadId, long threadMillis);

        void recordEarlyToplevelEndEvent(
                String name, long timeNanos, int threadId, long threadMillis);

        void recordEarlyAsyncBeginEvent(String name, long id, long timeNanos);

        void recordEarlyAsyncEndEvent(long id, long timeNanos);
    }
}
