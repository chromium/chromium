// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.SuppressLint;
import android.os.Build;
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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/**
 * Support for early tracing, before the native library is loaded.
 *
 * This is limited, as:
 * - Arguments are not supported
 * - Thread time is not reported
 * - Two events with the same name cannot be in progress at the same time.
 *
 * Events recorded here are buffered in Java until the native library is available. Then it waits
 * for the completion of pending events, and sends the events to the native side.
 *
 * Locking: This class is threadsafe. It is enabled when general tracing is, and then disabled when
 *          tracing is enabled from the native side. Event completions are still processed as long
 *          as some are pending, then early tracing is permanently disabled after dumping the
 *          events.  This means that if any early event is still pending when tracing is disabled,
 *          all early events are dropped.
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
        final String mName;
        final int mThreadId;
        final long mBeginTimeNanos;
        final long mBeginThreadTimeMillis;
        long mEndTimeNanos;
        long mEndThreadTimeMillis;

        Event(String name) {
            mName = name;
            mThreadId = Process.myTid();
            mBeginTimeNanos = elapsedRealtimeNanos();
            mBeginThreadTimeMillis = SystemClock.currentThreadTimeMillis();
        }

        void end() {
            assert mEndTimeNanos == 0;
            assert mEndThreadTimeMillis == 0;
            mEndTimeNanos = elapsedRealtimeNanos();
            mEndThreadTimeMillis = SystemClock.currentThreadTimeMillis();
        }

        @VisibleForTesting
        @SuppressLint("NewApi")
        static long elapsedRealtimeNanos() {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
                return SystemClock.elapsedRealtimeNanos();
            } else {
                return SystemClock.elapsedRealtime() * 1000000;
            }
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
            mTimestampNanos = Event.elapsedRealtimeNanos();
        }
    }

    // State transitions are:
    // - enable(): DISABLED -> ENABLED
    // - disable(): ENABLED -> FINISHING
    // - Once there are no pending events: FINISHING -> FINISHED.
    @VisibleForTesting static final int STATE_DISABLED = 0;
    @VisibleForTesting static final int STATE_ENABLED = 1;
    @VisibleForTesting static final int STATE_FINISHING = 2;
    @VisibleForTesting static final int STATE_FINISHED = 3;

    private static final String BACKGROUND_STARTUP_TRACING_ENABLED_KEY = "bg_startup_tracing";
    private static boolean sCachedBackgroundStartupTracingFlag;

    // Locks the fields below.
    private static final Object sLock = new Object();

    @VisibleForTesting static volatile int sState = STATE_DISABLED;
    // Not final as these object are not likely to be used at all.
    @GuardedBy("sLock")
    @VisibleForTesting
    static List<Event> sCompletedEvents;
    @GuardedBy("sLock")
    @VisibleForTesting
    static Map<String, Event> sPendingEventByKey;
    @GuardedBy("sLock")
    @VisibleForTesting
    static List<AsyncEvent> sAsyncEvents;
    @GuardedBy("sLock")
    @VisibleForTesting
    static List<String> sPendingAsyncEvents;

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
                    shouldEnable = (new File(TRACE_CONFIG_FILENAME)).exists();
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

    @VisibleForTesting
    static void enable() {
        synchronized (sLock) {
            if (sState != STATE_DISABLED) return;
            sCompletedEvents = new ArrayList<Event>();
            sPendingEventByKey = new HashMap<String, Event>();
            sAsyncEvents = new ArrayList<AsyncEvent>();
            sPendingAsyncEvents = new ArrayList<String>();
            sState = STATE_ENABLED;
        }
    }

    /**
     * Disables Early tracing.
     *
     * Once this is called, no new event will be registered. However, end() calls are still recorded
     * as long as there are pending events. Once there are none left, pass the events to the native
     * side.
     */
    static void disable() {
        synchronized (sLock) {
            if (!enabled()) return;
            sState = STATE_FINISHING;
            maybeFinishLocked();
        }
    }

    /**
     * Returns whether early tracing is currently active.
     *
     * Active means that Early Tracing is either enabled or waiting to complete pending events.
     */
    static boolean isActive() {
        int state = sState;
        return (state == STATE_ENABLED || state == STATE_FINISHING);
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
    public static void begin(String name) {
        // begin() and end() are going to be called once per TraceEvent, this avoids entering a
        // synchronized block at each and every call.
        if (!enabled()) return;
        Event event = new Event(name);
        Event conflictingEvent;
        synchronized (sLock) {
            if (!enabled()) return;
            conflictingEvent = sPendingEventByKey.put(makeEventKeyForCurrentThread(name), event);
        }
        if (conflictingEvent != null) {
            throw new IllegalArgumentException(
                    "Multiple pending trace events can't have the same name: " + name);
        }
    }

    /** @see TraceEvent#end */
    public static void end(String name) {
        if (!isActive()) return;
        synchronized (sLock) {
            if (!isActive()) return;
            Event event = sPendingEventByKey.remove(makeEventKeyForCurrentThread(name));
            if (event == null) return;
            event.end();
            sCompletedEvents.add(event);
            if (sState == STATE_FINISHING) maybeFinishLocked();
        }
    }

    /** @see TraceEvent#startAsync */
    public static void startAsync(String name, long id) {
        if (!enabled()) return;
        AsyncEvent event = new AsyncEvent(name, id, true /*isStart*/);
        synchronized (sLock) {
            if (!enabled()) return;
            sAsyncEvents.add(event);
            sPendingAsyncEvents.add(name);
        }
    }

    /** @see TraceEvent#finishAsync */
    public static void finishAsync(String name, long id) {
        if (!isActive()) return;
        AsyncEvent event = new AsyncEvent(name, id, false /*isStart*/);
        synchronized (sLock) {
            if (!isActive()) return;
            if (!sPendingAsyncEvents.remove(name)) return;
            sAsyncEvents.add(event);
            if (sState == STATE_FINISHING) maybeFinishLocked();
        }
    }

    @VisibleForTesting
    static void resetForTesting() {
        synchronized (sLock) {
            sState = EarlyTraceEvent.STATE_DISABLED;
            sCompletedEvents = null;
            sPendingEventByKey = null;
            sAsyncEvents = null;
            sPendingAsyncEvents = null;
        }
    }

    @GuardedBy("sLock")
    private static void maybeFinishLocked() {
        if (!sCompletedEvents.isEmpty()) {
            dumpEvents(sCompletedEvents);
            sCompletedEvents.clear();
        }
        if (!sAsyncEvents.isEmpty()) {
            dumpAsyncEvents(sAsyncEvents);
            sAsyncEvents.clear();
        }
        if (sPendingEventByKey.isEmpty() && sPendingAsyncEvents.isEmpty()) {
            sState = STATE_FINISHED;
            sPendingEventByKey = null;
            sCompletedEvents = null;
            sPendingAsyncEvents = null;
            sAsyncEvents = null;
        }
    }

    private static void dumpEvents(List<Event> events) {
        long offsetNanos = getOffsetNanos();
        for (Event e : events) {
            EarlyTraceEventJni.get().recordEarlyEvent(e.mName, e.mBeginTimeNanos + offsetNanos,
                    e.mEndTimeNanos + offsetNanos, e.mThreadId,
                    e.mEndThreadTimeMillis - e.mBeginThreadTimeMillis);
        }
    }
    private static void dumpAsyncEvents(List<AsyncEvent> events) {
        long offsetNanos = getOffsetNanos();
        for (AsyncEvent e : events) {
            if (e.mIsStart) {
                EarlyTraceEventJni.get().recordEarlyStartAsyncEvent(
                        e.mName, e.mId, e.mTimestampNanos + offsetNanos);
            } else {
                EarlyTraceEventJni.get().recordEarlyFinishAsyncEvent(
                        e.mName, e.mId, e.mTimestampNanos + offsetNanos);
            }
        }
    }

    private static long getOffsetNanos() {
        long nativeNowNanos = TimeUtilsJni.get().getTimeTicksNowUs() * 1000;
        long javaNowNanos = Event.elapsedRealtimeNanos();
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
        void recordEarlyEvent(String name, long beginTimNanos, long endTimeNanos, int threadId,
                long threadDurationMillis);
        void recordEarlyStartAsyncEvent(String name, long id, long timestamp);
        void recordEarlyFinishAsyncEvent(String name, long id, long timestamp);
    }
}
