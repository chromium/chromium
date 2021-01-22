// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Process;
import android.os.StrictMode;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

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
@MainDex
public class EarlyTraceEvent {
    // Must be kept in sync with the native kAndroidTraceConfigFile.
    private static final String TRACE_CONFIG_FILENAME = "/data/local/chrome-trace-config.json";

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
            mTimeNanos = SystemClock.elapsedRealtimeNanos();
            mThreadTimeMillis = SystemClock.currentThreadTimeMillis();
        }
    }

    @VisibleForTesting
    static final class AsyncEvent {
        final boolean mIsStart;
        final String mName;
        final long mId;
        final long mTimestampNanos;

        AsyncEvent(String name, long id, boolean isStart) {
            mName = name;
            mId = id;
            mIsStart = isStart;
            mTimestampNanos = SystemClock.elapsedRealtimeNanos();
        }
    }

    // State transitions are:
    // - enable(): DISABLED -> ENABLED
    // - disable(): ENABLED -> FINISHED
    @VisibleForTesting static final int STATE_DISABLED = 0;
    @VisibleForTesting static final int STATE_ENABLED = 1;
    @VisibleForTesting
    static final int STATE_FINISHED = 2;

    private static final String BACKGROUND_STARTUP_TRACING_ENABLED_KEY = "bg_startup_tracing";
    private static boolean sCachedBackgroundStartupTracingFlag;

    // Locks the fields below.
    private static final Object sLock = new Object();

    @VisibleForTesting static volatile int sState = STATE_DISABLED;
    // Not final as these object are not likely to be used at all.
    @GuardedBy("sLock")
    @VisibleForTesting
    static List<Event> sEvents;
    @GuardedBy("sLock")
    @VisibleForTesting
    static List<AsyncEvent> sAsyncEvents;

    /** @see TraceEvent#maybeEnableEarlyTracing() */
    static void maybeEnable() {
        ThreadUtils.assertOnUiThread();
        if (sState != STATE_DISABLED) return;
        boolean shouldEnable = false;
        // Checking for the trace config filename touches the disk.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            if (CommandLine.getInstance().hasSwitch("trace-startup")) {
                shouldEnable = true;
            } else {
                try {
                    shouldEnable = new File(TRACE_CONFIG_FILENAME).exists();
                } catch (SecurityException e) {
                    // Access denied, not enabled.
                }
            }
            if (ContextUtils.getAppSharedPreferences().getBoolean(
                        BACKGROUND_STARTUP_TRACING_ENABLED_KEY, false)) {
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

    static boolean enabled() {
        return sState == STATE_ENABLED;
    }

    /**
     * Sets the background startup tracing enabled in app preferences for next startup.
     */
    @CalledByNative
    static void setBackgroundStartupTracingFlag(boolean enabled) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(BACKGROUND_STARTUP_TRACING_ENABLED_KEY, enabled)
                .apply();
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
        Event event = new Event(name, true /*isStart*/, isToplevel);
        synchronized (sLock) {
            if (!enabled()) return;
            sEvents.add(event);
        }
    }

    /** @see TraceEvent#end */
    public static void end(String name, boolean isToplevel) {
        if (!enabled()) return;
        Event event = new Event(name, false /*isStart*/, isToplevel);
        synchronized (sLock) {
            if (!enabled()) return;
            sEvents.add(event);
        }
    }

    /** @see TraceEvent#startAsync */
    public static void startAsync(String name, long id) {
        if (!enabled()) return;
        AsyncEvent event = new AsyncEvent(name, id, true /*isStart*/);
        synchronized (sLock) {
            if (!enabled()) return;
            sAsyncEvents.add(event);
        }
    }

    /** @see TraceEvent#finishAsync */
    public static void finishAsync(String name, long id) {
        if (!enabled()) return;
        AsyncEvent event = new AsyncEvent(name, id, false /*isStart*/);
        synchronized (sLock) {
            if (!enabled()) return;
            sAsyncEvents.add(event);
        }
    }

    @VisibleForTesting
    static void resetForTesting() {
        synchronized (sLock) {
            sState = EarlyTraceEvent.STATE_DISABLED;
            sEvents = null;
            sAsyncEvents = null;
        }
    }

    private static void dumpEvents(List<Event> events) {
        long offsetNanos = getOffsetNanos();
        for (Event e : events) {
            if (e.mIsStart) {
                if (e.mIsToplevel) {
                    EarlyTraceEventJni.get().recordEarlyToplevelBeginEvent(
                            e.mName, e.mTimeNanos + offsetNanos, e.mThreadId, e.mThreadTimeMillis);
                } else {
                    EarlyTraceEventJni.get().recordEarlyBeginEvent(
                            e.mName, e.mTimeNanos + offsetNanos, e.mThreadId, e.mThreadTimeMillis);
                }
            } else {
                if (e.mIsToplevel) {
                    EarlyTraceEventJni.get().recordEarlyToplevelEndEvent(
                            e.mName, e.mTimeNanos + offsetNanos, e.mThreadId, e.mThreadTimeMillis);
                } else {
                    EarlyTraceEventJni.get().recordEarlyEndEvent(
                            e.mName, e.mTimeNanos + offsetNanos, e.mThreadId, e.mThreadTimeMillis);
                }
            }
        }
    }
    private static void dumpAsyncEvents(List<AsyncEvent> events) {
        long offsetNanos = getOffsetNanos();
        for (AsyncEvent e : events) {
            if (e.mIsStart) {
                EarlyTraceEventJni.get().recordEarlyAsyncBeginEvent(
                        e.mName, e.mId, e.mTimestampNanos + offsetNanos);
            } else {
                EarlyTraceEventJni.get().recordEarlyAsyncEndEvent(
                        e.mName, e.mId, e.mTimestampNanos + offsetNanos);
            }
        }
    }

    private static long getOffsetNanos() {
        long nativeNowNanos = TimeUtilsJni.get().getTimeTicksNowUs() * 1000;
        long javaNowNanos = SystemClock.elapsedRealtimeNanos();
        return nativeNowNanos - javaNowNanos;
    }

    /**
     * Returns a key which consists of |name| and the ID of the current thread.
     * The key is used with pending events making them thread-specific, thus avoiding
     * an exception when similarly named events are started from multiple threads.
     */
    @VisibleForTesting
    static String makeEventKeyForCurrentThread(String name) {
        return name + "@" + Process.myTid();
    }

    @NativeMethods
    interface Natives {
        void recordEarlyBeginEvent(String name, long timeNanos, int threadId, long threadMillis);
        void recordEarlyEndEvent(String name, long timeNanos, int threadId, long threadMillis);
        void recordEarlyToplevelBeginEvent(
                String name, long timeNanos, int threadId, long threadMillis);
        void recordEarlyToplevelEndEvent(
                String name, long timeNanos, int threadId, long threadMillis);
        void recordEarlyAsyncBeginEvent(String name, long id, long timestamp);
        void recordEarlyAsyncEndEvent(String name, long id, long timestamp);
    }
}
