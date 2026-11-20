// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/**
 * Support class exposing {@link TraceEvent} methods to native unit tests ({@code
 * base/android/trace_event_binding_unittest.cc}) via {@code @CalledByNative} without adding
 * test-only JNI entry points to production {@link TraceEvent}.
 */
@NullMarked
@JNINamespace("base::android")
final class TraceEventNativeTestSupport {
    private TraceEventNativeTestSupport() {}

    @CalledByNative
    private static boolean enabled() {
        return TraceEvent.enabled();
    }

    @CalledByNative
    private static void setEnabled(boolean enabled) {
        TraceEvent.setEnabled(enabled);
    }

    @CalledByNative
    private static void webViewStartupTotalFactoryInit(long startTimeMs, long durationMs) {
        TraceEvent.webViewStartupTotalFactoryInit(startTimeMs, durationMs);
    }

    @CalledByNative
    private static void webViewStartupStage1(long startTimeMs, long durationMs) {
        TraceEvent.webViewStartupStage1(startTimeMs, durationMs);
    }

    @CalledByNative
    private static void webViewStartupFirstInstance(
            long startTimeMs, long durationMs, boolean includedGlobalStartup) {
        TraceEvent.webViewStartupFirstInstance(startTimeMs, durationMs, includedGlobalStartup);
    }

    @CalledByNative
    private static void webViewStartupNotFirstInstance(long startTimeMs, long durationMs) {
        TraceEvent.webViewStartupNotFirstInstance(startTimeMs, durationMs);
    }

    @CalledByNative
    private static void webViewStartupStartChromiumLocked(
            long startTimeMs,
            long durationMs,
            int startCallSite,
            int finishCallSite,
            int startupMode) {
        TraceEvent.webViewStartupStartChromiumLocked(
                startTimeMs, durationMs, startCallSite, finishCallSite, startupMode);
    }
}
