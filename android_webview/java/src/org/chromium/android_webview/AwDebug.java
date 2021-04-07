// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.metrics.RecordHistogram;

import java.io.File;

/**
 * Provides Android WebView debugging entrypoints.
 *
 * Methods in this class can be called from any thread, including threads created by
 * the client of WebView.
 */
@JNINamespace("android_webview")
@UsedByReflection("")
public class AwDebug {
    private static final String TAG = "AwDebug";

    /**
     * Previously requested to dump WebView state as a minidump.
     *
     * This is no longer supported as it doesn't include renderer state in
     * multiprocess mode, significantly limiting its usefulness.
     */
    @UsedByReflection("")
    public static boolean dumpWithoutCrashing(File dumpFile) {
        Log.e(TAG, "AwDebug.dumpWithoutCrashing is no longer supported.");
        return false;
    }

    public static void setSupportLibraryWebkitVersionCrashKey(String version) {
        AwDebugJni.get().setSupportLibraryWebkitVersionCrashKey(version);
    }

    // Used to record the UMA histogram Android.WebView.AwDebugCall. Since these values are
    // persisted to logs, they should never be renumbered or reused.
    @IntDef({AwDebugCall.SET_CPU_AFFINITY_TO_LITTLE_CORES, AwDebugCall.ENABLE_IDLE_THROTTLING})
    @interface AwDebugCall {
        int SET_CPU_AFFINITY_TO_LITTLE_CORES = 0;
        int ENABLE_IDLE_THROTTLING = 1;
        int COUNT = 2;
    }

    @UsedByReflection("")
    public static void setCpuAffinityToLittleCores() {
        RecordHistogram.recordEnumeratedHistogram("Android.WebView.AwDebugCall",
                AwDebugCall.SET_CPU_AFFINITY_TO_LITTLE_CORES, AwDebugCall.COUNT);
        AwDebugJni.get().setCpuAffinityToLittleCores();
    }

    @UsedByReflection("")
    public static void enableIdleThrottling() {
        RecordHistogram.recordEnumeratedHistogram("Android.WebView.AwDebugCall",
                AwDebugCall.ENABLE_IDLE_THROTTLING, AwDebugCall.COUNT);
        AwDebugJni.get().enableIdleThrottling();
    }

    @NativeMethods
    interface Natives {
        void setSupportLibraryWebkitVersionCrashKey(String version);
        void setCpuAffinityToLittleCores();
        void enableIdleThrottling();
    }
}
