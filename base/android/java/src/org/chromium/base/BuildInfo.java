// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.compat.ApiHelperForP;

/**
 * BuildInfo is a utility class providing easy access to {@link PackageInfo} information. This is
 * primarily of use for accessing package information from native code.
 */
public class BuildInfo {
    private static final String TAG = "BuildInfo";
    private static final int MAX_FINGERPRINT_LENGTH = 128;

    private static PackageInfo sBrowserPackageInfo;
    private static boolean sInitialized;

    /** Not a member variable to avoid creating the instance early (it is set early in start up). */
    private static String sFirebaseAppId = "";

    /** The application name (e.g. "Chrome"). For WebView, this is name of the embedding app. */
    public final String hostPackageLabel;
    /** By default: same as versionCode. For WebView: versionCode of the embedding app. */
    public final long hostVersionCode;
    /** The packageName of Chrome/WebView. Use application context for host app packageName. */
    public final String packageName;
    /** The versionCode of the apk. */
    public final long versionCode;
    /** The versionName of Chrome/WebView. Use application context for host app versionName. */
    public final String versionName;
    /** Result of PackageManager.getInstallerPackageName(). Never null, but may be "". */
    public final String installerPackageName;
    /** The versionCode of Play Services (for crash reporting). */
    public final String gmsVersionCode;
    /** Formatted ABI string (for crash reporting). */
    public final String abiString;
    /** Truncated version of Build.FINGERPRINT (for crash reporting). */
    public final String androidBuildFingerprint;
    /** A string that is different each time the apk changes. */
    public final String extractedFileSuffix;
    /** Whether or not the device has apps installed for using custom themes. */
    public final String customThemes;
    /** Product version as stored in Android resources. */
    public final String resourcesVersion;

    private static class Holder { private static BuildInfo sInstance = new BuildInfo(); }

    @CalledByNative
    private static String[] getAll() {
        BuildInfo buildInfo = getInstance();
        String hostPackageName = ContextUtils.getApplicationContext().getPackageName();
        return new String[] {
                Build.BRAND,
                Build.DEVICE,
                Build.ID,
                Build.MANUFACTURER,
                Build.MODEL,
                String.valueOf(Build.VERSION.SDK_INT),
                Build.TYPE,
                Build.BOARD,
                hostPackageName,
                String.valueOf(buildInfo.hostVersionCode),
                buildInfo.hostPackageLabel,
                buildInfo.packageName,
                String.valueOf(buildInfo.versionCode),
                buildInfo.versionName,
                buildInfo.androidBuildFingerprint,
                buildInfo.gmsVersionCode,
                buildInfo.installerPackageName,
                buildInfo.abiString,
                sFirebaseAppId,
                buildInfo.customThemes,
                buildInfo.resourcesVersion,
                buildInfo.extractedFileSuffix,
                isAtLeastQ() ? "1" : "0",
                isDebugAndroid() ? "1" : "0",
        };
    }

    private static String nullToEmpty(CharSequence seq) {
        return seq == null ? "" : seq.toString();
    }

    private static long packageVersionCode(PackageInfo pi) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return ApiHelperForP.getLongVersionCode(pi);
        } else {
            return pi.versionCode;
        }
    }

    /**
     * @param packageInfo Package for Chrome/WebView (as opposed to host app).
     */
    public static void setBrowserPackageInfo(PackageInfo packageInfo) {
        assert !sInitialized;
        sBrowserPackageInfo = packageInfo;
    }

    public static BuildInfo getInstance() {
        return Holder.sInstance;
    }

    private BuildInfo() {
        sInitialized = true;
        try {
            Context appContext = ContextUtils.getApplicationContext();
            String hostPackageName = appContext.getPackageName();
            PackageManager pm = appContext.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(hostPackageName, 0);
            hostVersionCode = packageVersionCode(pi);
            if (sBrowserPackageInfo != null) {
                packageName = sBrowserPackageInfo.packageName;
                versionCode = packageVersionCode(sBrowserPackageInfo);
                versionName = nullToEmpty(sBrowserPackageInfo.versionName);
                sBrowserPackageInfo = null;
            } else {
                packageName = hostPackageName;
                versionCode = hostVersionCode;
                versionName = nullToEmpty(pi.versionName);
            }

            hostPackageLabel = nullToEmpty(pm.getApplicationLabel(pi.applicationInfo));
            installerPackageName = nullToEmpty(pm.getInstallerPackageName(packageName));

            PackageInfo gmsPackageInfo = null;
            try {
                gmsPackageInfo = pm.getPackageInfo("com.google.android.gms", 0);
            } catch (NameNotFoundException e) {
                Log.d(TAG, "GMS package is not found.");
            }
            gmsVersionCode = gmsPackageInfo != null
                    ? String.valueOf(packageVersionCode(gmsPackageInfo))
                    : "gms versionCode not available.";

            String hasCustomThemes = "true";
            try {
                // Substratum is a theme engine that enables users to use custom themes provided
                // by theme apps. Sometimes these can cause crashs if not installed correctly.
                // These crashes can be difficult to debug, so knowing if the theme manager is
                // present on the device is useful (http://crbug.com/820591).
                pm.getPackageInfo("projekt.substratum", 0);
            } catch (NameNotFoundException e) {
                hasCustomThemes = "false";
            }
            customThemes = hasCustomThemes;

            String currentResourcesVersion = "Not Enabled";
            // Controlled by target specific build flags.
            if (BuildConfig.R_STRING_PRODUCT_VERSION != 0) {
                try {
                    // This value can be compared with the actual product version to determine if
                    // corrupted resources were the cause of a crash. This can happen if the app
                    // loads resources from the outdated package  during an update
                    // (http://crbug.com/820591).
                    currentResourcesVersion = ContextUtils.getApplicationContext().getString(
                            BuildConfig.R_STRING_PRODUCT_VERSION);
                } catch (Exception e) {
                    currentResourcesVersion = "Not found";
                }
            }
            resourcesVersion = currentResourcesVersion;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                abiString = TextUtils.join(", ", Build.SUPPORTED_ABIS);
            } else {
                abiString = String.format("ABI1: %s, ABI2: %s", Build.CPU_ABI, Build.CPU_ABI2);
            }

            // Append lastUpdateTime to versionCode, since versionCode is unlikely to change when
            // developing locally but lastUpdateTime is.
            extractedFileSuffix = String.format("@%x_%x", versionCode, pi.lastUpdateTime);

            // The value is truncated, as this is used for crash and UMA reporting.
            androidBuildFingerprint = Build.FINGERPRINT.substring(
                    0, Math.min(Build.FINGERPRINT.length(), MAX_FINGERPRINT_LENGTH));
        } catch (NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Check if this is a debuggable build of Android. Use this to enable developer-only features.
     * This is a rough approximation of the hidden API {@code Build.IS_DEBUGGABLE}.
     */
    public static boolean isDebugAndroid() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    // The markers Begin:BuildCompat and End:BuildCompat delimit code
    // that is autogenerated from Android sources.
    // Begin:BuildCompat Q

    /**
     * Checks if the device is running on a pre-release version of Android Q or newer.
     * <p>
     * <strong>Note:</strong> This method will return {@code false} on devices running release
     * versions of Android. When Android Q is finalized for release, this method will be deprecated
     * and all calls should be replaced with {@code Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q}.
     *
     * @return {@code true} if Q APIs are available for use, {@code false} otherwise
     */
    public static boolean isAtLeastQ() {
        return Build.VERSION.SDK_INT >= 29;
    }

    /**
     * Checks if the application targets pre-release SDK Q
     */
    public static boolean targetsAtLeastQ() {
        return ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion >= 29;
    }

    public static void setFirebaseAppId(String id) {
        assert sFirebaseAppId.equals("");
        sFirebaseAppId = id;
    }

    public static String getFirebaseAppId() {
        return sFirebaseAppId;
    }

    // End:BuildCompat
}
