// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A wrapper class around {@link org.chromium.base.TraceEvent} and {@link
 * org.chromium.base.metrics.ScopedSysTraceEvent} that allows both traces to be emitted with a
 * single scoped variable.
 *
 * <p>This class is used by WebView to maintain tracing to atrace during startup while also emitting
 * traces to Perfetto through {@link org.chromium.base.EarlyTraceEvent}.
 */
@NullMarked
public final class DualTraceEvent implements AutoCloseable {

    private final @Nullable TraceEvent mTraceEvent;
    private final @Nullable ScopedSysTraceEvent mSysTraceEvent;

    public static DualTraceEvent scoped(String name) {
        return new DualTraceEvent(TraceEvent.scoped(name), ScopedSysTraceEvent.scoped(name));
    }

    private DualTraceEvent(@Nullable TraceEvent traceEvent, ScopedSysTraceEvent sysTraceEvent) {
        this.mTraceEvent = traceEvent;
        this.mSysTraceEvent = sysTraceEvent;
    }

    @Override
    public void close() {
        if (mTraceEvent != null) {
            mTraceEvent.close();
        }
        if (mSysTraceEvent != null) {
            mSysTraceEvent.close();
        }
    }
}
