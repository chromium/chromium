// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Build;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** AndroidInfo is a utility class to access Android's Build information specific data. */
@JNINamespace("base::android::android_info")
@NullMarked
public final class AndroidInfo {
    private static final String TAG = "AndroidInfo";

    // Called by the native code to retrieve field values. There is no easy way to
    // return several fields from Java to native, so instead this calls back into
    // native, passing the fields as parameters to a native function.
    // The native code expects native `fillFields()` to be called inline from this
    // function.
    @CalledByNative
    private static void nativeReadyForFields() {
        AndroidInfoJni.get()
                .fillFields(
                        /* brand= */ Build.BRAND,
                        /* device= */ Build.DEVICE,
                        /* buildId= */ Build.ID,
                        /* manufacturer= */ Build.MANUFACTURER,
                        /* model= */ Build.MODEL,
                        /* type= */ Build.TYPE,
                        /* board= */ Build.BOARD,
                        /* androidBuildFingerprint= */ getAndroidBuildFingerprint(),
                        /* versionIncremental= */ Build.VERSION.INCREMENTAL,
                        /* hardware= */ Build.HARDWARE,
                        /* codeName= */ Build.VERSION.CODENAME,
                        /* socManufacturer= */ Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                                ? Build.SOC_MANUFACTURER
                                : "",
                        /* supportedAbis= */ TextUtils.join(", ", Build.SUPPORTED_ABIS),
                        /* sdkInt= */ Build.VERSION.SDK_INT,
                        /* isDebugAndroid= */ isDebugAndroid(),
                        /* securityPatch= */ Build.VERSION.SECURITY_PATCH);
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

    @NativeMethods
    interface Natives {
        void fillFields(
                @JniType("std::string") String brand,
                @JniType("std::string") String device,
                @JniType("std::string") String buildId,
                @JniType("std::string") String manufacturer,
                @JniType("std::string") String model,
                @JniType("std::string") String type,
                @JniType("std::string") String board,
                @JniType("std::string") String androidBuildFingerprint,
                @JniType("std::string") String versionIncremental,
                @JniType("std::string") String hardware,
                @JniType("std::string") String codeName,
                @JniType("std::string") String socManufacturer,
                @JniType("std::string") String supportedAbis,
                int sdkInt,
                boolean isDebugAndroid,
                @JniType("std::string") String securityPatch);
    }
}
