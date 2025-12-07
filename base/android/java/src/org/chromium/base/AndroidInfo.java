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
        sendToNative(getAidlInfo());
    }

    public static void sendToNative(IAndroidInfo info) {
        AndroidInfoJni.get()
                .fillFields(
                        /* brand= */ info.brand,
                        /* device= */ info.device,
                        /* buildId= */ info.androidBuildId,
                        /* manufacturer= */ info.manufacturer,
                        /* model= */ info.model,
                        /* type= */ info.buildType,
                        /* board= */ info.board,
                        /* androidBuildFingerprint= */ info.androidBuildFp,
                        /* versionIncremental= */ info.versionIncremental,
                        /* hardware= */ info.hardware,
                        /* codename= */ info.codename,
                        /* socManufacturer= */ info.socManufacturer,
                        /* supportedAbis= */ info.abiName,
                        /* sdkInt= */ info.sdkInt,
                        /* isDebugAndroid= */ info.isDebugAndroid,
                        /* securityPatch= */ info.securityPatch);
    }

    public static IAndroidInfo getAidlInfo() {
        IAndroidInfo info = new IAndroidInfo();
        info.abiName = getAndroidSupportedAbis();
        info.androidBuildFp = getAndroidBuildFingerprint();
        info.androidBuildId = Build.ID;
        info.board = Build.BOARD;
        info.brand = Build.BRAND;
        info.buildType = Build.TYPE;
        info.codename = Build.VERSION.CODENAME;
        info.device = Build.DEVICE;
        info.hardware = Build.HARDWARE;
        info.isDebugAndroid = isDebugAndroid();
        info.manufacturer = Build.MANUFACTURER;
        info.model = Build.MODEL;
        info.sdkInt = Build.VERSION.SDK_INT;
        info.securityPatch = Build.VERSION.SECURITY_PATCH;
        info.socManufacturer =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? Build.SOC_MANUFACTURER : "";
        info.versionIncremental = Build.VERSION.INCREMENTAL;
        return info;
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
        // Note - this is called very early in WebView startup, before the command line is
        // initialized or our application context is ready. Do not change this implementation to
        // rely on anything beyond what the Android system provides.
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
                @JniType("std::string") String codename,
                @JniType("std::string") String socManufacturer,
                @JniType("std::string") String supportedAbis,
                int sdkInt,
                boolean isDebugAndroid,
                @JniType("std::string") String securityPatch);
    }
}
