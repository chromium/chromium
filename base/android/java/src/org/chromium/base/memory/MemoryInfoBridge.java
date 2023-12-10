// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory;

import android.app.ActivityManager;
import android.content.Context;
import android.os.Debug;
import android.os.Process;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;

/** Allows calling ActivityManager#getProcessMemoryInfo() from native. */
public class MemoryInfoBridge {
    /**
     * Returns the result of ActivityManager#getProcessMemoryInfo() on itself.
     *
     * Calls to this method are heavily throttled in ActivityManager, with stale results silently
     * returned when called too often. Should not be called outside of the native caller, as the
     * throttling handling code there would become incorrect otherwise.
     */
    @CalledByNative
    public static @Nullable Debug.MemoryInfo getActivityManagerMemoryInfoForSelf() {
        ActivityManager activityManager =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        int pid = Process.myPid();
        try {
            Debug.MemoryInfo[] infos = activityManager.getProcessMemoryInfo(new int[] {pid});
            return infos == null ? null : infos[0];
        } catch (SecurityException e) {
            // Isolated callers are not allowed. Since this is used for logging only, don't crash in
            // this case. Also, prevents issues if the framework further restricts this API (which
            // has happened, with a restriction on PIDs starting in Q).
            return null;
        }
    }
}
