// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.Process;

import org.jni_zero.JniType;

import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.build.annotations.NullMarked;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo} information. This is
 * primarily of use for accessing package information from native code.
 *
 * <p>NOTE: This class is deprecated, You can find the appropriate utilities that lived here in
 * AndroidInfo, ApkInfo or DeviceInfo.
 */
@NullMarked
@Deprecated
public class BuildInfo {
    private static final String TAG = "BuildInfo";

    /**
     * The package name of the host app which has loaded WebView, retrieved from the application
     * context. In the context of the SDK Runtime, the package name of the app that owns this
     * particular instance of the SDK Runtime will also be included. e.g.
     * com.google.android.sdksandbox:com:com.example.myappwithads
     */
    public final String hostPackageName;

    /**
     * The application name (e.g. "Chrome"). For WebView, this is name of the embedding app.
     * In the context of the SDK Runtime, this is the name of the app that owns this particular
     * instance of the SDK Runtime.
     */
    public final String hostPackageLabel;

    /**
     * By default: same as versionCode. For WebView: versionCode of the embedding app. In the
     * context of the SDK Runtime, this is the versionCode of the app that owns this particular
     * instance of the SDK Runtime.
     */
    public final String hostVersionCode;

    /**
     * The packageName of Chrome/WebView. Use application context for host app packageName. Same as
     * the host information within any child process.
     */
    public final String packageName;

    /** The versionCode of the apk. */
    public final long versionCode = BuildConfig.VERSION_CODE;

    /** The versionName of Chrome/WebView. Use application context for host app versionName. */
    public final String versionName;

    /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
    public final String installerPackageName;

    /** Formatted ABI string (for crash reporting). */
    public final String abiString;

    /** Truncated version of Build.FINGERPRINT (for crash reporting). */
    public final String androidBuildFingerprint;

    /** Product version as stored in Android resources. */
    public final String resourcesVersion;

    /** Whether we're running on Android TV or not */
    public final boolean isTV;

    /** Whether we're running on an Android Automotive OS device or not. */
    public final boolean isAutomotive;

    /** Whether we're running on an Android Foldable OS device or not. */
    public final boolean isFoldable;

    /**
     * version of the FEATURE_VULKAN_DEQP_LEVEL, if available. Queried only on Android T or above
     */
    public final int vulkanDeqpLevel;

    private static class Holder {
        private static final BuildInfo INSTANCE = new BuildInfo();
    }

    public static void setGmsVersionCodeForTest(@JniType("std::string") String gmsVersionCode) {
        DeviceInfo.setGmsVersionCodeForTest(gmsVersionCode);
    }

    /**
     * Return the "long" version code of the given PackageInfo. Does the right thing for
     * before/after Android P when this got wider.
     */
    public static long packageVersionCode(PackageInfo pi) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return pi.getLongVersionCode();
        } else {
            return pi.versionCode;
        }
    }

    /**
     * @return CPU architecture name, see "arch:" in:
     *     https://chromium.googlesource.com/chromium/src.git/+/master/docs/updater/protocol_3_1.md
     */
    public static String getArch() {
        boolean is64Bit = Process.is64Bit();
        if (NativeLibraries.sCpuFamily == NativeLibraries.CPU_FAMILY_ARM) {
            return is64Bit ? "arm64" : "arm";
        } else if (NativeLibraries.sCpuFamily == NativeLibraries.CPU_FAMILY_X86) {
            return is64Bit ? "x86_64" : "x86";
        }
        return "";
    }

    /**
     * @param packageInfo Package for Chrome/WebView (as opposed to host app).
     */
    public static void setBrowserPackageInfo(PackageInfo packageInfo) {
        ApkInfo.setBrowserPackageInfo(packageInfo);
    }

    /**
     * @return ApplicationInfo for Chrome/WebView (as opposed to host app).
     */
    public ApplicationInfo getBrowserApplicationInfo() {
        return ApkInfo.getInstance().getBrowserApplicationInfo();
    }

    public static BuildInfo getInstance() {
        // Some tests mock out things BuildInfo is based on, so disable caching in tests to ensure
        // such mocking is not defeated by caching.
        if (BuildConfig.IS_FOR_TEST) {
            return new BuildInfo();
        }
        return Holder.INSTANCE;
    }

    /** The versionCode of Play Services. */
    public String getGmsVersionCode() {
        return DeviceInfo.getGmsVersionCode();
    }

    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/98
    private BuildInfo() {
        hostPackageName = ApkInfo.getHostPackageName();
        hostPackageLabel = ApkInfo.getHostPackageLabel();
        hostVersionCode = ApkInfo.getHostVersionCode();
        packageName = ApkInfo.getPackageName();
        versionName = ApkInfo.getPackageVersionName();
        installerPackageName = ApkInfo.getInstallerPackageName();
        abiString = AndroidInfo.getAndroidSupportedAbis();
        androidBuildFingerprint = AndroidInfo.getAndroidBuildFingerprint();
        resourcesVersion = ApkInfo.getResourcesVersion();
        isTV = DeviceInfo.isTV();
        isAutomotive = DeviceInfo.isAutomotive();
        isFoldable = DeviceInfo.isFoldable();
        vulkanDeqpLevel = DeviceInfo.getVulkanDeqpLevel();
    }

    /**
     * Check if this is a debuggable build of Android. This is a rough approximation of the hidden
     * API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return AndroidInfo.isDebugAndroid();
    }

    /*
     * Check if the app is declared debuggable in its manifest.
     * In WebView, this refers to the host app.
     */
    public static boolean isDebugApp() {
        return ApkInfo.isDebugApp();
    }

    /**
     * Check if this is either a debuggable build of Android or of the host app. Use this to enable
     * developer-only features.
     */
    public static boolean isDebugAndroidOrApp() {
        return isDebugAndroid() || isDebugApp();
    }
}
