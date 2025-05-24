// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.os.Trace;

import org.chromium.build.annotations.NullMarked;

/**
 * An alternative to @{TraceEvent} that allows us to trace events before native initialization.
 *
 * <p>Note that TraceEvent / EarlyTraceEvent cannot be used before native initialization since it
 * directly purges to the kernel debug message but that method does not allow tracing events to be
 * written *after* the event occurrence.
 */
@NullMarked
public class ScopedSysTraceEvent implements AutoCloseable {
    /** The maximum length of a section name. Longer names will be truncated. */
    // From:
    // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/os/Trace.java;l=115;drc=cad0f6adc5e8ca56f9a35a20f23ddd87c13af33e
    public static final int MAX_SECTION_NAME_LEN = 127;

    /**
     * Factory used to support the "try with resource" construct. Note that currently this is the
     * only allowed pattern. However, this requires heap allocation so we may consider calling
     * Trace.beginSection() / endSection() directly if it should be used repeatedly.
     *
     * @param name Trace event name.
     * @return a {@ScopedSysTraceEvent}, or null if tracing is not enabled.
     */
    public static ScopedSysTraceEvent scoped(String name) {
        return new ScopedSysTraceEvent(name);
    }

    /** Constructor used to support the "try with resource" construct. */
    private ScopedSysTraceEvent(String name) {
        // Section names longer than MAX_SECTION_NAME_LEN will throw, see:
        // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/os/Trace.java;l=471;drc=cad0f6adc5e8ca56f9a35a20f23ddd87c13af33e
        // This can easily lead to surprise crashes given the section name may be dynamically
        // constructed, and the exception is only thrown when tracing is enabled. Mitigate the
        // impact by truncating the section name.
        if (name.length() > MAX_SECTION_NAME_LEN) {
            final var ellipsis = "...";
            name = name.substring(0, MAX_SECTION_NAME_LEN - ellipsis.length()) + ellipsis;
        }
        Trace.beginSection(name);
    }

    @Override
    public void close() {
        Trace.endSection();
    }
}
