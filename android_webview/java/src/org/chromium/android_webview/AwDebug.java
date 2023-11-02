// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.UsedByReflection;

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

    @NativeMethods
    interface Natives {
        void setSupportLibraryWebkitVersionCrashKey(String version);
    }
}
