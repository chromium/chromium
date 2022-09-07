// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.os.Trace;

/**
 * An alternative to @{TraceEvent} that allows us to trace events before native
 * initialization.
 *
 * Note that TraceEvent / EarlyTraceEvent cannot be used before native initialization since
 * it directly purges to the kernel debug message but that method does not allow tracing events
 * to be written *after* the event occurrence.
 */
public class ScopedSysTraceEvent implements AutoCloseable {
    /**
     * Factory used to support the "try with resource" construct.
     * Note that currently this is the only allowed pattern. However, this requires heap allocation
     * so we may consider calling Trace.beginSection() / endSection() directly if it should be used
     * repeatedly.
     *
     * @param name Trace event name.
     * @return a {@ScopedSysTraceEvent}, or null if tracing is not enabled.
     */
    public static ScopedSysTraceEvent scoped(String name) {
        return new ScopedSysTraceEvent(name);
    }

    /**
     * Constructor used to support the "try with resource" construct.
     */
    private ScopedSysTraceEvent(String name) {
        Trace.beginSection(name);
    }

    @Override
    public void close() {
        Trace.endSection();
    }
}