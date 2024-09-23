// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Build;
import android.os.StrictMode;

import org.chromium.build.BuildConfig;

import java.io.Closeable;

/**
 * Enables try-with-resources compatible StrictMode violation allowlisting.
 *
 * <p>Prefer "ignored" as the variable name to appease Android Studio's "Unused symbol" inspection.
 *
 * <p>Example:
 *
 * <pre>
 *     try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
 *         return Example.doThingThatRequiresDiskWrites();
 *     }
 * </pre>
 */
public class StrictModeContext implements Closeable {
    private static class Impl extends StrictModeContext {
        private final StrictMode.ThreadPolicy mThreadPolicy;
        private final StrictMode.VmPolicy mVmPolicy;

        private Impl(StrictMode.ThreadPolicy threadPolicy, StrictMode.VmPolicy vmPolicy) {
            mThreadPolicy = threadPolicy;
            mVmPolicy = vmPolicy;
        }

        private Impl(StrictMode.ThreadPolicy threadPolicy) {
            this(threadPolicy, null);
        }

        private Impl(StrictMode.VmPolicy vmPolicy) {
            this(null, vmPolicy);
        }

        @Override
        public void close() {
            if (mThreadPolicy != null) {
                StrictMode.setThreadPolicy(mThreadPolicy);
            }
            if (mVmPolicy != null) {
                StrictMode.setVmPolicy(mVmPolicy);
            }
            TraceEvent.finishAsync("StrictModeContext", hashCode());
        }
    }

    /**
     * Convenience method for disabling all VM-level StrictMode checks with try-with-resources.
     * Includes everything listed here:
     * https://developer.android.com/reference/android/os/StrictMode.VmPolicy.Builder.html
     */
    public static StrictModeContext allowAllVmPolicies() {
        if (BuildConfig.DISABLE_STRICT_MODE_CONTEXT) {
            return new StrictModeContext();
        }
        try (TraceEvent e = TraceEvent.scoped("StrictModeContext.allowAllVmPolicies")) {
            StrictMode.VmPolicy oldPolicy = StrictMode.getVmPolicy();
            StrictMode.setVmPolicy(StrictMode.VmPolicy.LAX);
            return new Impl(oldPolicy);
        }
    }

    /**
     * Convenience method for disabling all thread-level StrictMode checks with try-with-resources.
     * Includes everything listed here:
     * https://developer.android.com/reference/android/os/StrictMode.ThreadPolicy.Builder.html
     */
    public static StrictModeContext allowAllThreadPolicies() {
        if (BuildConfig.DISABLE_STRICT_MODE_CONTEXT) {
            return new StrictModeContext();
        }
        try (TraceEvent e = TraceEvent.scoped("StrictModeContext.allowAllThreadPolicies")) {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
            StrictMode.setThreadPolicy(StrictMode.ThreadPolicy.LAX);
            return new Impl(oldPolicy);
        }
    }

    /** Convenience method for disabling StrictMode for disk-writes with try-with-resources. */
    public static StrictModeContext allowDiskWrites() {
        if (BuildConfig.DISABLE_STRICT_MODE_CONTEXT) {
            return new StrictModeContext();
        }
        try (TraceEvent e = TraceEvent.scoped("StrictModeContext.allowDiskWrites")) {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            return new Impl(oldPolicy);
        }
    }

    /** Convenience method for disabling StrictMode for disk-reads with try-with-resources. */
    public static StrictModeContext allowDiskReads() {
        if (BuildConfig.DISABLE_STRICT_MODE_CONTEXT) {
            return new StrictModeContext();
        }
        try (TraceEvent e = TraceEvent.scoped("StrictModeContext.allowDiskReads")) {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            return new Impl(oldPolicy);
        }
    }

    /** Convenience method for disabling StrictMode for slow calls with try-with-resources. */
    public static StrictModeContext allowSlowCalls() {
        if (BuildConfig.DISABLE_STRICT_MODE_CONTEXT) {
            return new StrictModeContext();
        }
        try (TraceEvent e = TraceEvent.scoped("StrictModeContext.allowSlowCalls")) {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
            StrictMode.setThreadPolicy(
                    new StrictMode.ThreadPolicy.Builder(oldPolicy).permitCustomSlowCalls().build());
            return new Impl(oldPolicy);
        }
    }

    /**
     * Convenience method for disabling StrictMode for unbuffered input/output operations with
     * try-with-resources. For API level 25- this method will do nothing; because
     * StrictMode.ThreadPolicy.Builder#permitUnbufferedIo is added in API level 26.
     */
    public static StrictModeContext allowUnbufferedIo() {
        if (BuildConfig.DISABLE_STRICT_MODE_CONTEXT) {
            return new StrictModeContext();
        }
        try (TraceEvent e = TraceEvent.scoped("StrictModeContext.allowUnbufferedIo")) {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                StrictMode.setThreadPolicy(
                        new StrictMode.ThreadPolicy.Builder(oldPolicy)
                                .permitUnbufferedIo()
                                .build());
            }
            return new Impl(oldPolicy);
        }
    }

    @Override
    public void close() {}
}
