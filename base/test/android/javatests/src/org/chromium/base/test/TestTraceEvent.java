// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;

/**
 * TestTraceEvent is a modified version of TraceEvent, intended for tracing test runs.
 */
public class TestTraceEvent {
    private static final String TAG = "TestTraceEvent";

    /** The event types understood by the trace scripts. */
    private enum EventType {
        BEGIN("B"),
        END("E"),
        INSTANT("I");

        private final String mTypeStr;

        EventType(String typeStr) {
            mTypeStr = typeStr;
        }

        @Override
        public String toString() {
            return mTypeStr;
        }
    }

    // Locks internal fields.
    private static final Object sLock = new Object();

    private static File sOutputFile;

    private static boolean sEnabled;

    // A list of trace event strings.
    private static JSONArray sTraceStrings;

    /**
     * Enable tracing, and set a specific output file. If tracing was previously enabled and
     * disabled, that data is cleared.
     *
     * @param file Which file to append the trace data to.
     */
    public static void enable(File outputFile) {
        synchronized (sLock) {
            if (sEnabled) return;

            sEnabled = true;
            sOutputFile = outputFile;
            sTraceStrings = new JSONArray();
        }
    }

    /**
     * Disabling of tracing will dump trace data to the system log.
     */
    public static void disable() {
        synchronized (sLock) {
            if (!sEnabled) return;

            sEnabled = false;
            dumpTraceOutput();
            sTraceStrings = null;
        }
    }

    /**
     * @return True if tracing is enabled, false otherwise.
     */
    public static boolean isEnabled() {
        synchronized (sLock) {
            return sEnabled;
        }
    }

    /**
     * Record an "instant" trace event. E.g. "screen update happened".
     */
    public static void instant(String name) {
        synchronized (sLock) {
            if (!sEnabled) return;

            saveTraceString(name, name.hashCode(), EventType.INSTANT);
        }
    }

    /**
     * Record an "begin" trace event. Begin trace events should have a matching end event (recorded
     * by calling {@link #end(String)}).
     */
    public static void begin(String name) {
        synchronized (sLock) {
            if (!sEnabled) return;

            saveTraceString(name, name.hashCode(), EventType.BEGIN);
        }
    }

    /**
     * Record an "end" trace event, to match a begin event (recorded by calling {@link
     * #begin(String)}). The time delta between begin and end is usually interesting to graph code.
     */
    public static void end(String name) {
        synchronized (sLock) {
            if (!sEnabled) return;

            saveTraceString(name, name.hashCode(), EventType.END);
        }
    }

    /**
     * Save a trace event as a JSON dict.
     *
     * @param name The trace data.
     * @param id An identifier for the event, to be saved as the thread ID.
     * @param type the type of trace event (B, E, I).
     */
    private static void saveTraceString(String name, long id, EventType type) {
        // We use System.currentTimeMillis() because it agrees with the value of
        // the $EPOCHREALTIME environment variable. The Python test runner code
        // uses that variable to synchronize timing.
        long timeMicroseconds = System.currentTimeMillis() * 1000;

        try {
            JSONObject traceObj = new JSONObject();
            traceObj.put("cat", "Java");
            traceObj.put("ts", timeMicroseconds);
            traceObj.put("ph", type);
            traceObj.put("name", name);
            traceObj.put("tid", id);

            sTraceStrings.put(traceObj);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Dump all tracing data we have saved up to the log.
     * Output as JSON for parsing convenience.
     */
    private static void dumpTraceOutput() {
        try {
            PrintStream stream = new PrintStream(new FileOutputStream(sOutputFile, true));
            try {
                stream.print(sTraceStrings);
            } finally {
                if (stream != null) stream.close();
            }
        } catch (FileNotFoundException ex) {
            Log.e(TAG, "Unable to dump trace data to output file.");
        }
    }
}
