// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;

/** AndroidInfo is a utility class to access Android's Build information specific data. */
public class AndroidInfo {
    private static final String TAG = "AndroidInfo";

    // Returns Android info fields in a specific order that the caller is expected to follow.
    // This is awkward and error-prone, but the alternative of having one JNI method per
    // field was deemed too inefficient in terms of binary size.
    @CalledByNative
    private static String[] getStringAndroidInfo() {
        return new String[] {
            Build.BRAND,
            Build.DEVICE,
            Build.ID,
            Build.MANUFACTURER,
            Build.MODEL,
            Build.TYPE,
            Build.BOARD,
            getAndroidBuildFingerprint(),
            Build.VERSION.INCREMENTAL,
            Build.HARDWARE,
            Build.VERSION.CODENAME,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? Build.SOC_MANUFACTURER : "",
            TextUtils.join(", ", Build.SUPPORTED_ABIS)
        };
    }

    // Returns Android info fields in a specific order that the caller is expected to follow.
    // This is awkward and error-prone, but the alternative of having one JNI method per
    // field was deemed too inefficient in terms of binary size.
    @CalledByNative
    private static int[] getIntAndroidInfo() {
        return new int[] {
            Build.VERSION.SDK_INT,
            isDebugAndroid() ? 1 : 0,
            Build.VERSION.SDK_INT >= VERSION_CODES.UPSIDE_DOWN_CAKE ? 1 : 0,
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU ? 1 : 0,
        };
    }

    /* Truncated version of Build.FINGERPRINT (for crash reporting). */
    public static String getAndroidBuildFingerprint() {
        return Build.FINGERPRINT.substring(0, Math.min(Build.FINGERPRINT.length(), 128));
    }

    public static String getAndroidSupportedAbis() {
        return TextUtils.join(", ", Build.SUPPORTED_ABIS);
    }

    private AndroidInfo() {}

    /**
     * Check if this is a debuggable build of Android. This is a rough approximation of the hidden
     * API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }
}
